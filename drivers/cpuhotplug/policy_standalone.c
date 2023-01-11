/* linux/driver/cpuhotplug/policies/standalone.c
 *
 *  Copyright (C) 2012 Marvell, Inc.
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

#include <linux/platform_device.h>
#include <linux/tick.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/cpunum_qos.h>
#ifndef CONFIG_ARM64
#include <linux/cputype.h>
#endif
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <trace/events/pxa.h>

#define BOOT_DELAY	60
#define CHECK_DELAY_NOP	(.5 * HZ * 16)
#define CHECK_DELAY_ON	(.5 * HZ * 4)
#define CHECK_DELAY_OFF	(.5 * HZ)
#define TRANS_RQ	2
#define TRANS_LOAD_RQ	20
#define NUM_CPUS	num_possible_cpus()
#define NUM_ON_CPUS	num_online_cpus()
#define CPULOAD_TABLE	(NR_CPUS + 1)
#define KHZ_TO_HZ	(1000)

#define cputime64_sub(__a, __b)	((__a) - (__b))
static struct workqueue_struct *hotplug_wq;
static struct delayed_work hotplug_work;

static unsigned int freq_max;
static unsigned int freq_min = -1;
static unsigned int max_performance;

static unsigned int hotpluging_rate = CHECK_DELAY_OFF;
static unsigned int hotpluging_nop_time = CHECK_DELAY_NOP;
static unsigned int hotpluging_out_time = CHECK_DELAY_ON;
static unsigned int nop_time;
static unsigned int out_time;
static unsigned int user_lock;
static unsigned int trans_rq = TRANS_RQ;
static unsigned int trans_load_rq = TRANS_LOAD_RQ;

static unsigned int trans_load_l0;
static unsigned int trans_load_l1;
static unsigned int trans_load_l2;
static unsigned int trans_load_l3;
static unsigned int trans_load_h0;
static unsigned int trans_load_h1;
static unsigned int trans_load_h2;
static unsigned int trans_load_h3;

static struct kobject hotplug_kobj;

enum flag {
	HOTPLUG_NOP,
	HOTPLUG_IN,
	HOTPLUG_OUT
};

enum {
	LOW = 0,
	HIGH,
};

struct cpu_time_info {
	cputime64_t prev_cpu_idle;
	cputime64_t prev_cpu_wall;
	cputime64_t total_wall_time;	/* True wall time */
	unsigned int load;	/* Not consider freq */
	unsigned int avg_load;	/* Consider freq */
};

struct cpu_hotplug_info {
	unsigned long nr_running;
	pid_t tgid;
};

static DEFINE_PER_CPU(struct cpu_time_info, hotplug_cpu_time);

static u32 trans_load[][2] = {
	{150000, 312000},	/* pxa1928 */
	{624000, 800000},	/* pxa1908 */
	{150000, 312000},	/* pxa1L88 */
};

/*
 * Mutex can be used since hotplug_timer does not run in
 * timer(softirq) context but in process context.
 */
static DEFINE_MUTEX(hotplug_stat_lock);
static DEFINE_MUTEX(hotplug_user_lock);
static struct clk *cpu_clk;

static void init_hotplug_statistics(void)
{
	int i;

	/* initialize tmp_info */
	for_each_online_cpu(i) {
		struct cpu_time_info *tmp_info;
		cputime64_t cur_wall_time, cur_idle_time;
		tmp_info = &per_cpu(hotplug_cpu_time, i);

		/* get current idle and wall time */
		cur_idle_time = get_cpu_idle_time(i, &cur_wall_time, 0);

		/* initial tmp_info */
		tmp_info->load = 0;
		tmp_info->avg_load = 0;
		tmp_info->prev_cpu_idle = cur_idle_time;
		tmp_info->prev_cpu_wall = cur_wall_time;
		tmp_info->total_wall_time = cur_wall_time;
	}

	/* initialize nop_time, out_time */
	nop_time = 0;
	out_time = 0;
}

static inline enum flag
standalone_hotplug(unsigned int avg_load, unsigned int cur_freq,
		   unsigned long nr_rq_min, unsigned int cpu_rq_min)
{
	unsigned int nr_online_cpu;
	enum flag st_flag = HOTPLUG_NOP;

	/* load threshold */
	unsigned int threshold[CPULOAD_TABLE][2] = {
		{trans_load_l0, trans_load_h0},
		{trans_load_l1, trans_load_h1},
		{trans_load_l2, trans_load_h2},
		{trans_load_l3, trans_load_h3},
	};

	nr_online_cpu = num_online_cpus();

	if (avg_load < threshold[nr_online_cpu - 1][LOW]) {
		nop_time = 0;
		if (nr_online_cpu > 1)
			st_flag = HOTPLUG_OUT;
		/*
		 * If total nr_running is less than cpu(on-state) number
		 * hotplug do not hotplug-in.
		 */
	} else if (avg_load > threshold[nr_online_cpu - 1][HIGH]) {
		nop_time = 0;
		if ((nr_running() > nr_online_cpu) && (cur_freq > freq_min))
			st_flag = HOTPLUG_IN;
	} else if (nr_online_cpu > 1 && nr_rq_min < trans_rq) {
		struct cpu_time_info *tmp_info;

		tmp_info = &per_cpu(hotplug_cpu_time, cpu_rq_min);
		/*
		 * If CPU(cpu_rq_min) load is less than trans_load_rq
		 * hotplug-out operation need.
		 */
		if ((tmp_info->load < trans_load_rq) &&
		    (avg_load < threshold[nr_online_cpu - 2][HIGH])) {
			nop_time = 0;
			st_flag = HOTPLUG_OUT;
		}
	} else if (nr_online_cpu > 1) {
		if ((avg_load >=
		     threshold[nr_online_cpu - 2][HIGH]) &&
		    (avg_load <= threshold[nr_online_cpu - 1][HIGH]))
			nop_time = 0;
		else if ((avg_load >=
			  threshold[nr_online_cpu - 1][LOW]) &&
			 (avg_load < threshold[nr_online_cpu - 2][HIGH])) {
			nop_time += hotpluging_rate;

			/*
			 * If load_l <= avg_load <= former load_h,
			 * more than one online cpu, and nop_time
			 * >= hotpluging_nop_time, plug-out cpu.
			 */
			if (nop_time >= hotpluging_nop_time) {
				nop_time = 0;
				return HOTPLUG_OUT;
			}
		}
	}

	/* calculate out_time */
	switch (st_flag) {
	case HOTPLUG_IN:
	case HOTPLUG_NOP:
		/* if plug-in or nop, clear out_time */
		out_time = 0;
		break;
	case HOTPLUG_OUT:
		/* if plug-out, compare out_time with rate */
		if ((out_time + hotpluging_rate) < hotpluging_out_time) {
			out_time += hotpluging_rate;
			st_flag = HOTPLUG_NOP;
		} else
			out_time = 0;
		break;
	}

	return st_flag;
}

static int hotplug_freq_notifier_call(struct notifier_block *nb,
				      unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	int i;

	if (val != CPUFREQ_POSTCHANGE)
		return 0;

	/* if lock this hotplug, user_lock=1, return */
	if (user_lock == 1)
		return 0;

	mutex_lock(&hotplug_stat_lock);

	for_each_online_cpu(i) {
		struct cpu_time_info *tmp_info;
		cputime64_t cur_wall_time, cur_idle_time;
		unsigned int idle_time, wall_time;

		tmp_info = &per_cpu(hotplug_cpu_time, i);

		/* get idle time and wall time */
		cur_idle_time = get_cpu_idle_time(i, &cur_wall_time, 0);

		/* update idle time */
		idle_time = (unsigned int)cputime64_sub(cur_idle_time,
							tmp_info->
							prev_cpu_idle);
		tmp_info->prev_cpu_idle = cur_idle_time;

		/* update wall time */
		wall_time = (unsigned int)cputime64_sub(cur_wall_time,
							tmp_info->
							prev_cpu_wall);
		tmp_info->prev_cpu_wall = cur_wall_time;

		/*
		 * let idle & wall time to be divided by 1024
		 * to avoid overflow
		 */
		idle_time >>= 10;
		wall_time >>= 10;

		/* update load */
		tmp_info->load += (wall_time - idle_time);

		/*
		 * update avg_load, and let freq to be divided
		 * by 1024 to avoid overflow
		 */
		tmp_info->avg_load += ((wall_time - idle_time)
				       * (freq->old >> 10));
	}

	mutex_unlock(&hotplug_stat_lock);

	return 0;
}

static int cpufreq_qos_max_notify(struct notifier_block *nb,
				      unsigned long max, void *data)
{
	mutex_lock(&hotplug_stat_lock);
	max_performance = NUM_CPUS * max;
	mutex_unlock(&hotplug_stat_lock);
	return NOTIFY_OK;
}

void hotplug_sync_idle_time(int cpu)
{
	struct cpu_time_info *tmp_info;
	cputime64_t cur_wall_time, cur_idle_time;

	mutex_lock(&hotplug_stat_lock);
	tmp_info = &per_cpu(hotplug_cpu_time, cpu);

	/* get idle time and wall time */
	cur_idle_time = get_cpu_idle_time(cpu, &cur_wall_time, 0);

	/* update idle time and wall time */
	tmp_info->prev_cpu_idle = cur_idle_time;
	tmp_info->prev_cpu_wall = cur_wall_time;
	tmp_info->total_wall_time = cur_wall_time;

	tmp_info->load = 0;
	tmp_info->avg_load = 0;

	mutex_unlock(&hotplug_stat_lock);
}

static int __cpuinit sthp_cpu_callback(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;

	if (user_lock == 1)
		return NOTIFY_OK;

	switch (action) {
	case CPU_UP_PREPARE:
		hotplug_sync_idle_time(cpu);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block hotplug_freq_notifier = {
	.notifier_call = hotplug_freq_notifier_call
};

static struct notifier_block  __refdata sthp_cpu_notifier = {
	.notifier_call = sthp_cpu_callback,
};

static struct notifier_block cpufreq_qos_max_notifier = {
	.notifier_call = cpufreq_qos_max_notify,
};

static void __ref hotplug_timer(struct work_struct *work)
{
	struct cpu_hotplug_info tmp_hotplug_info[4];
	int i;
	unsigned int avg_load = 0;
	unsigned int cpu_rq_min = 0;
	unsigned long nr_rq_min = -1UL;
	unsigned int select_off_cpu = 0;
	unsigned int cur_freq;
	enum flag flag_hotplug;
	s32 qos_min = 0;
	s32 qos_max = 0;

	/* if lock this hotplug, user_lock = 1, return */
	if (user_lock == 1)
		return;

	mutex_lock(&hotplug_stat_lock);

	/* get current cpu freq */
	cur_freq = clk_get_rate(cpu_clk) / KHZ_TO_HZ;

	for_each_online_cpu(i) {
		struct cpu_time_info *tmp_info;
		cputime64_t cur_wall_time, cur_idle_time;
		unsigned int idle_time, wall_time, total_wall_time;

		tmp_info = &per_cpu(hotplug_cpu_time, i);

		/* get idle time and wall time */
		cur_idle_time = get_cpu_idle_time(i, &cur_wall_time, 0);

		/* get idle_time and wall_time */
		idle_time = (unsigned int)cputime64_sub(cur_idle_time,
							tmp_info->
							prev_cpu_idle);
		wall_time =
		    (unsigned int)cputime64_sub(cur_wall_time,
						tmp_info->prev_cpu_wall);

		/* check wall time and idle time */
		if (wall_time < idle_time) {
			mutex_unlock(&hotplug_stat_lock);
			goto no_hotplug;
		}

		/* update idle time and wall time */
		tmp_info->prev_cpu_idle = cur_idle_time;
		tmp_info->prev_cpu_wall = cur_wall_time;

		/* update total wall time */
		total_wall_time = (unsigned int)cputime64_sub(cur_wall_time,
							      tmp_info->
							      total_wall_time);
		tmp_info->total_wall_time = cur_wall_time;

		/*
		 * let idle time, wall time and total wall time
		 * to be divided by 1024 to avoid overflow
		 */
		idle_time >>= 10;
		wall_time >>= 10;
		total_wall_time >>= 10;

		/* for once divide-by-zero issue */
		if (total_wall_time == 0)
			total_wall_time++;

		/* update load */
		tmp_info->load += (wall_time - idle_time);
		/* get real load(xx%) */
		tmp_info->load = 100 * tmp_info->load / total_wall_time;

		/*
		 * update avg_load on current freq, let freq to
		 * be divied by 1024 to avoid overflow
		 */
		tmp_info->avg_load += ((wall_time - idle_time)
				       * (cur_freq >> 10));

		/* get real avg_load(xx%) */
		tmp_info->avg_load = ((100 * tmp_info->avg_load)
				      / total_wall_time)
				      / (max_performance >> 10);

		trace_pxa_hp_single(i, get_cpu_nr_running(i), tmp_info->load,
				    (tmp_info->avg_load *
				     max_performance >> 10) / 100);

		/* get avg_load of two cores */
		avg_load += tmp_info->avg_load;

		/* find minimum runqueue length */
		tmp_hotplug_info[i].nr_running = get_cpu_nr_running(i);

		if ((i > 0) && (nr_rq_min > tmp_hotplug_info[i].nr_running)) {
			nr_rq_min = tmp_hotplug_info[i].nr_running;

			cpu_rq_min = i;
		}
	}

	trace_pxa_hp_total((cur_freq / 1000),
			   (avg_load * (max_performance >> 10) / 100),
			   num_online_cpus());

	for (i = NUM_CPUS - 1; i > 0; --i) {
		if (cpu_online(i) == 0) {
			select_off_cpu = i;
			break;
		}
	}

	/* standallone hotplug */
	flag_hotplug = standalone_hotplug(avg_load, cur_freq,
					  nr_rq_min, cpu_rq_min);

	/* initial tmp_info */
	for_each_online_cpu(i) {
		struct cpu_time_info *tmp_info;
		tmp_info = &per_cpu(hotplug_cpu_time, i);

		/* initial load, avg_load, total wall time */
		tmp_info->load = 0;
		tmp_info->avg_load = 0;
	}

	/* cpu hotplug */
	if ((flag_hotplug == HOTPLUG_IN) &&
	    /* avoid running cpu_up(0) */
	    select_off_cpu && (!cpu_online(select_off_cpu))) {
		pr_debug("cpu%d turning on!\n", select_off_cpu);
		mutex_unlock(&hotplug_stat_lock);

		cpunum_qos_lock();
		qos_max = pm_qos_request(PM_QOS_CPU_NUM_MAX);
		if (NUM_ON_CPUS < qos_max) {
			/* plug-in one cpu */
			cpu_up(select_off_cpu);
			pr_debug("cpu%d on\n", select_off_cpu);
		} else
			pr_debug("cpu%d on violate QoS(cur:%d, QoS:%d)\n",
				 select_off_cpu, NUM_ON_CPUS, qos_max);
		cpunum_qos_unlock();
	} else if ((flag_hotplug == HOTPLUG_OUT) &&
		   /* avoid running cpu_down(0) */
		   cpu_rq_min && (cpu_online(cpu_rq_min))) {
		pr_debug("cpu%d turnning off!\n", cpu_rq_min);
		mutex_unlock(&hotplug_stat_lock);

		cpunum_qos_lock();
		qos_min = pm_qos_request(PM_QOS_CPU_NUM_MIN);
		if (NUM_ON_CPUS > qos_min) {
			/* plug-out one cpu */
			cpu_down(cpu_rq_min);
			pr_debug("cpu%d off!\n", cpu_rq_min);
		} else
			pr_debug("cpu%d off violate QoS(cur:%d, QoS:%d)\n",
				 cpu_rq_min, NUM_ON_CPUS, qos_min);
		cpunum_qos_unlock();
	} else
		mutex_unlock(&hotplug_stat_lock);

no_hotplug:
	queue_delayed_work_on(0, hotplug_wq, &hotplug_work, hotpluging_rate);
}

static int standalone_hotplug_notifier_event(struct notifier_block *this,
					     unsigned long event, void *ptr)
{
	static unsigned user_lock_saved;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&hotplug_user_lock);
		user_lock_saved = user_lock;
		user_lock = 1;
		pr_info("%s: saving pm_hotplug lock %x\n",
			__func__, user_lock_saved);
		mutex_unlock(&hotplug_user_lock);
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		mutex_lock(&hotplug_user_lock);
		pr_info("%s: restoring pm_hotplug lock %x\n",
			__func__, user_lock_saved);
		user_lock = user_lock_saved;
		mutex_unlock(&hotplug_user_lock);

		/*
		 * corner case:
		 * user_lock set to 1 during suspend, and work_queue may goto
		 * "unlock", then work_queue never have chance to run again
		 */
		if (0 == user_lock) {
			flush_delayed_work(&hotplug_work);
			mutex_lock(&hotplug_stat_lock);
			/* Initialize data */
			init_hotplug_statistics();
			mutex_unlock(&hotplug_stat_lock);
			queue_delayed_work_on(0, hotplug_wq, &hotplug_work,
					      hotpluging_rate);
		}

		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

/* TODO: Whether the PM notifier is enabled in our suspend process */
static struct notifier_block standalone_hotplug_notifier = {
	.notifier_call = standalone_hotplug_notifier_event,
};

static int hotplug_reboot_notifier_call(struct notifier_block *this,
					unsigned long code, void *_cmd)
{
	mutex_lock(&hotplug_user_lock);
	pr_err("%s: disabling pm hotplug\n", __func__);
	user_lock = 1;
	mutex_unlock(&hotplug_user_lock);

	return NOTIFY_DONE;
}

static struct notifier_block hotplug_reboot_notifier = {
	.notifier_call = hotplug_reboot_notifier_call,
};

static ssize_t bound_freq_get(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	return sprintf(buf, "%d\n", freq_min);
}

static ssize_t bound_freq_set(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	int freq_tmp;
	if (1 != sscanf(buf, "%d", &freq_tmp))
		return -EINVAL;
	freq_min = freq_tmp;
	return count;
}

static DEVICE_ATTR(bound_freq, S_IRUGO | S_IWUSR, bound_freq_get,
		   bound_freq_set);

static ssize_t lock_get(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	return sprintf(buf, "%d\n", user_lock);
}

static ssize_t lock_set(struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t count)
{
	u32 val;
	int restart_hp = 0;
	bool waitqueue_finish = false;

	if (1 != sscanf(buf, "%d", &val))
		return -EINVAL;

	mutex_lock(&hotplug_user_lock);
	/* we want to re-enable governor */
	if ((1 == user_lock) && (0 == val))
		restart_hp = 1;
	/*
	 * when lock governor, we should wait workqueue finish
	 * before return, it can avoid user lock and gonvernor work
	 * concurrent conflict corner case
	 */
	if ((0 == user_lock) && (1 == !!val))
		waitqueue_finish = true;
	user_lock = val ? 1 : 0;
	mutex_unlock(&hotplug_user_lock);

	if (restart_hp) {
		flush_delayed_work(&hotplug_work);
		mutex_lock(&hotplug_stat_lock);
		/* initialize data */
		init_hotplug_statistics();
		mutex_unlock(&hotplug_stat_lock);
		queue_delayed_work_on(0, hotplug_wq, &hotplug_work,
				      CHECK_DELAY_OFF);
	}
	if (waitqueue_finish)
		cancel_delayed_work_sync(&hotplug_work);

	return count;
}

static DEVICE_ATTR(lock, S_IRUGO | S_IWUSR, lock_get, lock_set);

#define define_store_thhd_function(_name) \
static ssize_t load_set_##_name(struct device *dev, \
			struct device_attribute *attr, \
			const char *buf, size_t count) \
{ \
	int tmp; \
	if (1 != sscanf(buf, "%d", &tmp)) \
		return -EINVAL; \
	trans_load_##_name = tmp; \
	return count; \
}

#define define_show_thhd_function(_name) \
static ssize_t load_get_##_name(struct device *dev, \
				struct device_attribute *attr, \
				char *buf) \
{ \
	return sprintf(buf, "%d\n", (int) trans_load_##_name); \
}

define_store_thhd_function(l1);
define_store_thhd_function(l2);
define_store_thhd_function(l3);
define_store_thhd_function(h0);
define_store_thhd_function(h1);
define_store_thhd_function(h2);

define_show_thhd_function(l1);
define_show_thhd_function(l2);
define_show_thhd_function(l3);
define_show_thhd_function(h0);
define_show_thhd_function(h1);
define_show_thhd_function(h2);

static DEVICE_ATTR(load_l1, S_IRUGO | S_IWUSR, load_get_l1, load_set_l1);
static DEVICE_ATTR(load_l2, S_IRUGO | S_IWUSR, load_get_l2, load_set_l2);
static DEVICE_ATTR(load_l3, S_IRUGO | S_IWUSR, load_get_l3, load_set_l3);
static DEVICE_ATTR(load_h0, S_IRUGO | S_IWUSR, load_get_h0, load_set_h0);
static DEVICE_ATTR(load_h1, S_IRUGO | S_IWUSR, load_get_h1, load_set_h1);
static DEVICE_ATTR(load_h2, S_IRUGO | S_IWUSR, load_get_h2, load_set_h2);

static struct attribute *hotplug_attributes[] = {
	&dev_attr_lock.attr,
	&dev_attr_load_h0.attr,
	&dev_attr_load_l1.attr,
	&dev_attr_load_h1.attr,
	&dev_attr_load_l2.attr,
	&dev_attr_load_h2.attr,
	&dev_attr_load_l3.attr,
	&dev_attr_bound_freq.attr,
	NULL,
};

static struct kobj_type hotplug_dir_ktype = {
	.sysfs_ops = &kobj_sysfs_ops,
	.default_attrs = hotplug_attributes,
};

static int __init stand_alone_hotplug_init(void)
{
	unsigned int freq;
	int i, ret;
	struct cpufreq_frequency_table *table;

	pr_info("%s, PM-hotplug init function\n", __func__);

	hotplug_wq = create_singlethread_workqueue("dynamic hotplug");
	if (!hotplug_wq) {
		pr_err("Creation of hotplug work failed\n");
		ret = -EFAULT;
		goto err_create_singlethread_workqueue;
	}

	if (!cpu_clk) {
		cpu_clk = __clk_lookup("cpu");
		if (IS_ERR(cpu_clk)) {
			ret = PTR_ERR(cpu_clk);
			goto err_clk_get_sys;
		}
	}

	/* register cpufreq change notifier call */
	ret = cpufreq_register_notifier(&hotplug_freq_notifier,
					CPUFREQ_TRANSITION_NOTIFIER);
	if (ret)
		goto err_cpufreq_register_notifier;

	pm_qos_add_notifier(PM_QOS_CPUFREQ_MAX, &cpufreq_qos_max_notifier);

	/* register cpu hotplug notifier call */
	ret = register_hotcpu_notifier(&sthp_cpu_notifier);
	if (ret)
		goto err_hotplug_register_notifier;

	/* initialize data */
	init_hotplug_statistics();

	INIT_DELAYED_WORK(&hotplug_work, hotplug_timer);

	queue_delayed_work_on(0, hotplug_wq, &hotplug_work, BOOT_DELAY * HZ);
	table = cpufreq_frequency_get_table(0);

	for (i = 0; table[i].frequency != CPUFREQ_TABLE_END; i++) {
		freq = table[i].frequency;
		if (freq != CPUFREQ_ENTRY_INVALID && freq > freq_max)
			freq_max = freq;
		if (freq != CPUFREQ_ENTRY_INVALID && freq_min > freq)
			freq_min = freq;
	}

	max_performance = freq_max * NUM_CPUS;
	pr_info("init max_performance : %u\n", max_performance);

	if (of_machine_is_compatible("marvell,pxa1928"))
		i = 0;
	else if (of_machine_is_compatible("marvell,pxa1908"))
		i = 1;
	else
		i = 2;

	/* set trans_load_XX */
	trans_load_l0 = 0;
	trans_load_h0 = trans_load[i][HIGH] * 100 / max_performance;
	trans_load_l1 = trans_load[i][LOW] * 100 / max_performance;
	trans_load_h1 = trans_load[i][HIGH] * 100 * 2 / max_performance;
	trans_load_l2 = trans_load_h0;
	trans_load_h2 = trans_load[i][HIGH] * 100 * 3 / max_performance;
	trans_load_l3 = trans_load_h1;
	trans_load_h3 = 100;

	/* Show trans_load_XX */
	pr_info("--------------------\n"
		"|CPU|LOW(%%)|HIGH(%%)|\n"
		"|  1|   %3d|    %3d|\n"
		"|  2|   %3d|    %3d|\n"
		"|  3|   %3d|    %3d|\n"
		"|  4|   %3d|    %3d|\n"
		"--------------------\n",
		trans_load_l0, trans_load_h0, trans_load_l1, trans_load_h1,
		trans_load_l2, trans_load_h2, trans_load_l3, trans_load_h3);

	ret = kobject_init_and_add(&hotplug_kobj, &hotplug_dir_ktype,
				   &(cpu_subsys.dev_root->kobj), "hotplug");
	if (ret) {
		pr_err("%s: Failed to add kobject for hotplug\n", __func__);
		goto err_kobject_init_and_add;
	}

	ret = register_pm_notifier(&standalone_hotplug_notifier);
	if (ret)
		goto err_register_pm_notifier;

	ret = register_reboot_notifier(&hotplug_reboot_notifier);
	if (ret)
		goto err_register_reboot_notifier;

	return 0;

err_register_reboot_notifier:
	unregister_pm_notifier(&standalone_hotplug_notifier);
err_register_pm_notifier:
err_kobject_init_and_add:
	cancel_delayed_work(&hotplug_work);
	unregister_hotcpu_notifier(&sthp_cpu_notifier);
err_hotplug_register_notifier:
	pm_qos_remove_notifier(PM_QOS_CPUFREQ_MAX, &cpufreq_qos_max_notifier);
	cpufreq_unregister_notifier(&hotplug_freq_notifier,
				    CPUFREQ_TRANSITION_NOTIFIER);
err_cpufreq_register_notifier:
	clk_put(cpu_clk);
err_clk_get_sys:
	destroy_workqueue(hotplug_wq);
err_create_singlethread_workqueue:
	return ret;
}

module_init(stand_alone_hotplug_init);

static struct platform_device standalone_hotplug_device = {
	.name = "standalone-cpu-hotplug",
	.id = -1,
};

static int __init standalone_hotplug_device_init(void)
{
	int ret = 0;

	ret = platform_device_register(&standalone_hotplug_device);
	if (ret) {
		pr_err("Register device Failed\n");
		return ret;
	}
	pr_info("standalone_hotplug_device_init: %d\n", ret);

	return ret;
}

module_init(standalone_hotplug_device_init);
