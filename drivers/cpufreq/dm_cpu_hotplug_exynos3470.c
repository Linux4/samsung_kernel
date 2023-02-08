#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/fb.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sort.h>

#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>

#include <mach/cpufreq.h>
#include <linux/suspend.h>

#define NORMALMAX_FREQ	1400000
#define NORMALMIN_FREQ	400000
#define POLLING_MSEC	100

struct cpu_load_info {
	cputime64_t cpu_idle;
	cputime64_t cpu_iowait;
	cputime64_t cpu_wall;
	cputime64_t cpu_nice;
};

static DEFINE_PER_CPU(struct cpu_load_info, cur_cpu_info);

static DEFINE_MUTEX(dm_hotplug_lock);
static int dm_hotplug_enable_count;

static int cpu_util[NR_CPUS];
static struct pm_qos_request max_cpu_qos_hotplug;
static unsigned int cur_load_freq = 0;
static bool lcd_is_on;

enum hotplug_mode {
	CHP_NORMAL,
	CHP_LOW_POWER,
};

static enum hotplug_mode prev_mode;
static unsigned int delay = POLLING_MSEC;

static int dynamic_hotplug(enum hotplug_mode mode);

static inline void exynos_dm_hotplug_acquire_lock(void)
{
	mutex_lock(&dm_hotplug_lock);
}

static inline void exynos_dm_hotplug_release_lock(void)
{
	mutex_unlock(&dm_hotplug_lock);
}

static bool exynos_dm_hotplug_enabled(void)
{
	return (dm_hotplug_enable_count > 0);
}

static void exynos_dm_hotplug_enable(void)
{
	dm_hotplug_enable_count++;
}

static void exynos_dm_hotplug_disable(void)
{
	if (!exynos_dm_hotplug_enabled()) {
		pr_info("%s: dynamic hotplug already disabled\n",
				__func__);
		mutex_unlock(&dm_hotplug_lock);
		return;
	}
	dm_hotplug_enable_count--;
}

#ifdef CONFIG_PM
static ssize_t show_enable_dm_hotplug(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	bool enabled = exynos_dm_hotplug_enabled();

	return snprintf(buf, 10, "%s\n", enabled ? "enabled" : "disabled");
}

static ssize_t store_enable_dm_hotplug(struct kobject *kobj, struct attribute *attr,
		const char *buf, size_t count)
{
	int enable_input;

	if (!sscanf(buf, "%1d", &enable_input))
		return -EINVAL;

	if (enable_input > 1 || enable_input < 0) {
		pr_err("%s: invalid value (%d)\n", __func__, enable_input);
		return -EINVAL;
	}

	exynos_dm_hotplug_acquire_lock();
	if (enable_input) {
		exynos_dm_hotplug_enable();
	} else {
		dynamic_hotplug(CHP_NORMAL);
		exynos_dm_hotplug_disable();
	}
	exynos_dm_hotplug_release_lock();

	return count;
}

static struct global_attr enable_dm_hotplug =
__ATTR(enable_dm_hotplug, S_IRUGO | S_IWUSR,
		show_enable_dm_hotplug, store_enable_dm_hotplug);
#endif

static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu, u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time  = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = jiffies_to_usecs(cur_wall_time);

	return jiffies_to_usecs(idle_time);
}

static inline cputime64_t get_cpu_idle_time(unsigned int cpu, cputime64_t *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);
	else
		idle_time += get_cpu_iowait_time_us(cpu, wall);

	return idle_time;
}

static inline cputime64_t get_cpu_iowait_time(unsigned int cpu, cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

static inline void pm_qos_update_max(int frequency)
{
	if (pm_qos_request_active(&max_cpu_qos_hotplug))
		pm_qos_update_request(&max_cpu_qos_hotplug, frequency);
	else
		pm_qos_add_request(&max_cpu_qos_hotplug, PM_QOS_CPU_FREQ_MAX, frequency);
}

static int fb_state_change(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	unsigned int blank;

	if (val != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		lcd_is_on = false;
		pr_info("LCD is off\n");
		break;
	case FB_BLANK_UNBLANK:
		/*
		 * LCD blank CPU qos is set by exynos-ikcs-cpufreq
		 * This line of code release max limit when LCD is
		 * turned on.
		 */
		lcd_is_on = true;
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block fb_block = {
	.notifier_call = fb_state_change,
};

static int __ref __cpu_hotplug(struct cpumask *be_out_cpus)
{
	int i = 0;
	int ret = 0;

	if (!exynos_dm_hotplug_enabled() ||
			cpumask_weight(be_out_cpus) >= NR_CPUS) {
		ret = -EPERM;
		goto out;
	}

	for (i=1; i < NR_CPUS; i++) {
		if (cpumask_test_cpu(i, be_out_cpus)) {
			ret = cpu_down(i);
			if (ret)
				break;
		} else {
			ret = cpu_up(i);
			if (ret)
				break;
		}
	}
out:

	return ret;
}

static int dynamic_hotplug(enum hotplug_mode mode)
{
	int i;
	struct cpumask out_target;
	enum hotplug_mode ret = 0;

	cpumask_clear(&out_target);

	switch (mode) {
	case CHP_LOW_POWER:
		delay = POLLING_MSEC;
		for (i=1; i < NR_CPUS; i++)
			cpumask_set_cpu(i, &out_target);
		ret = __cpu_hotplug(&out_target);
		break;
	case CHP_NORMAL:
	default:
		delay = POLLING_MSEC;
		if (cpumask_weight(cpu_online_mask) < NR_CPUS)
			ret = __cpu_hotplug(&out_target);
		break;
	}

	return ret;
}

static int exynos_dm_hotplug_notifier(struct notifier_block *notifier,
					unsigned long pm_event, void *v)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		exynos_dm_hotplug_acquire_lock();
		exynos_dm_hotplug_disable();
		exynos_dm_hotplug_release_lock();
		break;
	case PM_POST_SUSPEND:
		exynos_dm_hotplug_acquire_lock();
		exynos_dm_hotplug_enable();
		exynos_dm_hotplug_release_lock();
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_dm_hotplug_nb = {
	.notifier_call = exynos_dm_hotplug_notifier,
	.priority = 1,
};

static int low_stay = 0;

static enum hotplug_mode diagnose_condition(void)
{
	int ret;

	ret = CHP_NORMAL;

	if (cur_load_freq > NORMALMIN_FREQ)
		low_stay = 0;
	else if (cur_load_freq <= NORMALMIN_FREQ && low_stay <= 5)
		low_stay++;
	if (low_stay > 5 && !lcd_is_on)
		ret = CHP_LOW_POWER;

	return ret;
}

static void calc_load(void)
{
	struct cpufreq_policy *policy;
	unsigned int cpu_util_sum = 0;
	int cpu = 0;
	unsigned int i;

	policy = cpufreq_cpu_get(cpu);

	if (!policy) {
		pr_err("Invalid policy\n");
		return;
	}

	cur_load_freq = 0;

	for_each_cpu(i, policy->cpus) {
		struct cpu_load_info	*i_load_info;
		cputime64_t cur_wall_time, cur_idle_time, cur_iowait_time;
		unsigned int idle_time, wall_time, iowait_time;
		unsigned int load, load_freq;

		i_load_info = &per_cpu(cur_cpu_info, i);

		cur_idle_time = get_cpu_idle_time(i, &cur_wall_time);
		cur_iowait_time = get_cpu_iowait_time(i, &cur_wall_time);

		wall_time = (unsigned int)
			(cur_wall_time - i_load_info->cpu_wall);
		i_load_info->cpu_wall = cur_wall_time;

		idle_time = (unsigned int)
			(cur_idle_time - i_load_info->cpu_idle);
		i_load_info->cpu_idle = cur_idle_time;

		iowait_time = (unsigned int)
			(cur_iowait_time - i_load_info->cpu_iowait);
		i_load_info->cpu_iowait = cur_iowait_time;

		if (unlikely(!wall_time || wall_time < idle_time))
			continue;

		load = 100 * (wall_time - idle_time) / wall_time;
		cpu_util[i] = load;
		cpu_util_sum += load;

		load_freq = load * policy->cur;

		if (policy->cur > cur_load_freq)
			cur_load_freq = policy->cur;
	}

	cpufreq_cpu_put(policy);
	return;
}

static int thread_run_flag;

static int on_run(void *data)
{
	int ret = 0;
	int on_cpu = 0;
	enum hotplug_mode exe_mode;

	struct cpumask thread_cpumask;

	cpumask_clear(&thread_cpumask);
	cpumask_set_cpu(on_cpu, &thread_cpumask);
	sched_setaffinity(0, &thread_cpumask);

	prev_mode = CHP_NORMAL;
	thread_run_flag = 1;

#ifdef CONFIG_PM
	ret = sysfs_create_file(power_kobj, &enable_dm_hotplug.attr);
	if (ret) {
		pr_err("%s: failed to create enable_dm_hotplug sysfs interface\n",
				__func__);
		return -EINVAL;
	}
#endif

	while (thread_run_flag) {
		calc_load();
		exe_mode = diagnose_condition();

		if (exe_mode != prev_mode) {
			exynos_dm_hotplug_acquire_lock();
			if (dynamic_hotplug(exe_mode) < 0)
				exe_mode = prev_mode;
			exynos_dm_hotplug_release_lock();
		}

		prev_mode = exe_mode;
		msleep(delay);
	}

	return 0;
}

void dm_cpu_hotplug_exit(void)
{
	thread_run_flag = 0;
}

void dm_cpu_hotplug_init(void)
{
	struct task_struct *k;

	k = kthread_run(&on_run, NULL, "thread_hotplug_func");
	if (IS_ERR(k))
		pr_err("Failed in creation of thread.\n");

	fb_register_client(&fb_block);
	lcd_is_on = true;

	dm_hotplug_enable_count = 1;
	register_pm_notifier(&exynos_dm_hotplug_nb);
}
