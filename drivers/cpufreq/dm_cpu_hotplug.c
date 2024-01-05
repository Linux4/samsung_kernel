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
#include <linux/reboot.h>

#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>

#include <mach/cpufreq.h>
#include <linux/suspend.h>

#if defined(CONFIG_SOC_EXYNOS5260)
#define NORMALMIN_FREQ	1000000
#else
#define NORMALMIN_FREQ	250000
#endif
#define POLLING_MSEC	100

struct cpu_load_info {
	cputime64_t cpu_idle;
	cputime64_t cpu_iowait;
	cputime64_t cpu_wall;
	cputime64_t cpu_nice;
};

static DEFINE_PER_CPU(struct cpu_load_info, cur_cpu_info);
static DEFINE_MUTEX(dm_hotplug_lock);
static DEFINE_MUTEX(thread_lock);
static DEFINE_MUTEX(big_hotplug_lock);
#if !defined(CONFIG_SCHED_HMP)
static DEFINE_MUTEX(core1_hotplug_in_lock);
#endif

static struct task_struct *dm_hotplug_task;
static int cpu_util[NR_CPUS];
static struct pm_qos_request max_cpu_qos_hotplug;
static unsigned int cur_load_freq = 0;
static bool lcd_is_on = true;
static bool in_low_power_mode = false;
static bool in_suspend_prepared = false;
static bool do_enable_hotplug = false;
static bool do_disable_hotplug = false;
#if defined(CONFIG_SCHED_HMP)
static bool do_hotplug_out = false;
static int big_hotpluged = 0;
#endif
#if !defined(CONFIG_SCHED_HMP)
static int cpu1_hotplug_in = 0;
#if defined(CONFIG_SOC_EXYNOS4415)
#define DEFAULT_NR_RUN_THRESHD  4
#define DEFAULT_NR_RUN_RANGE    2
#else
#define DEFAULT_NR_RUN_THRESHD  5
#define DEFAULT_NR_RUN_RANGE    2
#endif
static unsigned int nr_running_threshold = DEFAULT_NR_RUN_THRESHD;
static unsigned int nr_running_range = DEFAULT_NR_RUN_RANGE;
static unsigned int nr_running_count = 0;
static unsigned long cur_nr_running;
static bool core1_in_by_nr_running = false;
#endif

enum hotplug_cmd {
	CMD_NORMAL,
	CMD_LOW_POWER,
	CMD_BIG_IN,
	CMD_BIG_OUT,
	CMD_CORE_ONE_IN,
	CMD_CORE_ONE_OUT,
};

static int on_run(void *data);
static int dynamic_hotplug(enum hotplug_cmd cmd);

static enum hotplug_cmd prev_cmd = CMD_NORMAL;
static enum hotplug_cmd exe_cmd;
static unsigned int delay = POLLING_MSEC;

#if defined(CONFIG_SCHED_HMP)
static struct workqueue_struct *hotplug_wq;
#endif

static int dm_hotplug_disable = 0;

static bool exynos_dm_hotplug_disabled(void)
{
	return dm_hotplug_disable;
}

static void exynos_dm_hotplug_enable(void)
{
	mutex_lock(&dm_hotplug_lock);
	if (!exynos_dm_hotplug_disabled()) {
		pr_warn("%s: dynamic hotplug already enabled\n",
				__func__);
		mutex_unlock(&dm_hotplug_lock);
		return;
	}
	dm_hotplug_disable--;
	mutex_unlock(&dm_hotplug_lock);
}

static void exynos_dm_hotplug_disable(void)
{
	mutex_lock(&dm_hotplug_lock);
	dm_hotplug_disable++;
	mutex_unlock(&dm_hotplug_lock);
}

#if !defined(CONFIG_SCHED_HMP)
static int core1_hotplug_in(bool in_flag)
{
        int ret = 0;

        mutex_lock(&core1_hotplug_in_lock);

        if (in_flag) {
                if (cpu1_hotplug_in) {
                        cpu1_hotplug_in++;
                        goto out;
                }

                ret = dynamic_hotplug(CMD_CORE_ONE_IN);
                if (!ret)
                        cpu1_hotplug_in++;
        } else {
                if (WARN_ON(cpu1_hotplug_in == 0)) {
                        pr_err("%s: core1 already hotplug out\n",
                                        __func__);
                        ret = -EINVAL;
                        goto out;
                }

                if (cpu1_hotplug_in > 1) {
                        cpu1_hotplug_in--;
                        goto out;
                }

                ret = dynamic_hotplug(CMD_CORE_ONE_OUT);
                if (!ret)
                        cpu1_hotplug_in--;
        }

out:
        mutex_unlock(&core1_hotplug_in_lock);

        return ret;
}
#endif

#ifdef CONFIG_PM
static ssize_t show_enable_dm_hotplug(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	bool disabled = exynos_dm_hotplug_disabled();

	return snprintf(buf, 10, "%s\n", disabled ? "disabled" : "enabled");
}

static ssize_t store_enable_dm_hotplug(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	int enable_input;

	if (!sscanf(buf, "%d", &enable_input))
		return -EINVAL;

	if (enable_input > 1 || enable_input < 0) {
		pr_err("%s: invalid value (%d)\n", __func__, enable_input);
		return -EINVAL;
	}

	if (enable_input) {
		do_enable_hotplug = true;

		if (exynos_dm_hotplug_disabled())
			exynos_dm_hotplug_enable();
		else
			pr_info("%s: dynamic hotplug already enabled\n",
					__func__);

#if defined(CONFIG_SCHED_HMP)
		if (big_hotpluged)
			dynamic_hotplug(CMD_BIG_OUT);
#endif
		do_enable_hotplug = false;
	} else {
		do_disable_hotplug = true;
		if (!dynamic_hotplug(CMD_NORMAL))
			prev_cmd = CMD_NORMAL;
		if (!exynos_dm_hotplug_disabled())
			exynos_dm_hotplug_disable();
		else
			pr_info("%s: dynamic hotplug already disabled\n",
					__func__);
		do_disable_hotplug = false;
	}

	return count;
}

#if !defined(CONFIG_SCHED_HMP)
static ssize_t show_core1_hotplug_in(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	if (exynos_dm_hotplug_disabled())
		return snprintf(buf, PAGE_SIZE, "dynamic hotplug disabled\n");

	return snprintf(buf, PAGE_SIZE, "%s on low power mode\n",
			cpu1_hotplug_in ? "hotplug-in" : "hotplug-out");
}

static ssize_t store_core1_hotplug_in(struct kobject *kobj,
				struct attribute *attr, const char *buf, size_t count)
{
	int input_core1_hotplug_in;

	if (!sscanf(buf, "%1d", &input_core1_hotplug_in))
		return -EINVAL;

	if (input_core1_hotplug_in > 1 || input_core1_hotplug_in < 0) {
		pr_err("%s: invalid value (%d)\n", __func__, input_core1_hotplug_in);
		return -EINVAL;
	}

	core1_hotplug_in((bool)input_core1_hotplug_in);

	return count;
}

static ssize_t show_hotplug_nr_running(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "current_nr_running = %lu, "
			"nr_running_threshold = %u, nr_running_range = %u\n",
			cur_nr_running, nr_running_threshold, nr_running_range);
}

static ssize_t store_hotplug_nr_running(struct kobject *kobj,
				struct attribute *attr, const char *buf, size_t count)
{
	int input_nr_running_thrshd;
	int input_nr_running_range;

	if (!sscanf(buf, "%5d %5d", &input_nr_running_thrshd, &input_nr_running_range))
		return -EINVAL;

	if (input_nr_running_thrshd <= 1 || input_nr_running_range < 1) {
		pr_err("%s: invalid values (thrshd = %d, range = %d)\n",
			__func__, input_nr_running_thrshd, input_nr_running_range);
		pr_err("%s: thrshd is should be over than 1,"
			" and range is should be over than 0\n", __func__);
		return -EINVAL;
	}

	nr_running_threshold = (unsigned int)input_nr_running_thrshd;
	nr_running_range = (unsigned int)input_nr_running_range;

	pr_info("%s: nr_running_threshold = %u, nr_running_range = %u\n",
		__func__, nr_running_threshold, nr_running_range);

	return count;
}
#endif

static struct global_attr enable_dm_hotplug =
		__ATTR(enable_dm_hotplug, S_IRUGO | S_IWUSR,
			show_enable_dm_hotplug, store_enable_dm_hotplug);

#if !defined(CONFIG_SCHED_HMP)
static struct global_attr core_hotplug_in =
		__ATTR(core_hotplug_in, S_IRUGO | S_IWUSR,
			show_core1_hotplug_in, store_core1_hotplug_in);

static struct global_attr hotplug_nr_running =
		__ATTR(hotplug_nr_running, S_IRUGO | S_IWUSR,
			show_hotplug_nr_running, store_hotplug_nr_running);
#endif
#endif

#if defined(CONFIG_SEC_PM) && defined (CONFIG_SOC_EXYNOS4415)
static int threshold = 800000;

static ssize_t show_threshold_dm_hotplug(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", threshold );
}

static ssize_t store_threshold_dm_hotplug(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%10d", &input))
		return -EINVAL;

	if (input > 1600000 || input < 200000) {
		pr_err("%s: invalid value (%d)\n", __func__, input);
		return -EINVAL;
	}

        threshold = input;

	return count;
}


static struct global_attr threshold_dm_hotplug =
		__ATTR(threshold_dm_hotplug, S_IRUGO | S_IWUSR,
			show_threshold_dm_hotplug, store_threshold_dm_hotplug);
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
		pr_info("LCD is on\n");
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block fb_block = {
	.notifier_call = fb_state_change,
};

static int __ref __cpu_hotplug(bool out_flag, enum hotplug_cmd cmd)
{
	int i = 0;
	int ret = 0;
#if !defined(CONFIG_SCHED_HMP)
	int hotplug_out_limit = 0;
#endif

	if (exynos_dm_hotplug_disabled())
		return 0;

#if defined(CONFIG_SCHED_HMP)
	if (out_flag) {
		if (do_disable_hotplug)
			goto blk_out;

		if (cmd == CMD_BIG_OUT && !in_low_power_mode) {
			for (i = setup_max_cpus - 1; i >= NR_CA7; i--) {
				if (cpu_online(i)) {
					ret = cpu_down(i);
					if (ret)
						goto blk_out;
				}
			}
		} else {
			for (i = setup_max_cpus - 1; i > 0; i--) {
				if (cpu_online(i)) {
					ret = cpu_down(i);
					if (ret)
						goto blk_out;
				}
			}
		}
	} else {
		if (in_suspend_prepared)
			goto blk_out;

		if (cmd == CMD_BIG_IN) {
			if (in_low_power_mode)
				goto blk_out;

			for (i = NR_CA7; i < setup_max_cpus; i++) {
				if (!cpu_online(i)) {
					ret = cpu_up(i);
					if (ret)
						goto blk_out;
				}
			}
		} else {
			if (big_hotpluged && !do_disable_hotplug) {
				for (i = 1; i < NR_CA7; i++) {
					if (!cpu_online(i)) {
						ret = cpu_up(i);
						if (ret)
							goto blk_out;
					}
				}
			} else {
				for (i = 1; i < setup_max_cpus; i++) {
					if (do_hotplug_out && i >= NR_CA7)
						goto blk_out;

					if (!cpu_online(i)) {
						ret = cpu_up(i);
						if (ret)
							goto blk_out;
					}
				}
			}
		}
	}
#else
	if (out_flag) {
		if (do_disable_hotplug)
			goto blk_out;

		if (cmd == CMD_CORE_ONE_OUT) {
			if (!in_low_power_mode)
				goto blk_out;

			for (i = 1; i > 0; i--) {
				if (cpu_online(i)) {
					ret = cpu_down(i);
					if (ret)
						goto blk_out;
				}
			}
		} else {
			if (cpu1_hotplug_in)
				hotplug_out_limit = 1;

			for (i = setup_max_cpus - 1; i > hotplug_out_limit; i--) {
				if (cpu_online(i)) {
					ret = cpu_down(i);
					if (ret)
						goto blk_out;
				}
			}
		}
	} else {
		if (in_suspend_prepared)
			goto blk_out;

		if (cmd == CMD_CORE_ONE_IN) {
			for (i = 1; i < 2; i++) {
				if (!cpu_online(i)) {
					ret = cpu_up(i);
					if (ret)
						goto blk_out;
				}
			}
		} else {
			for (i = 1; i < setup_max_cpus; i++) {
				if (!cpu_online(i)) {
					ret = cpu_up(i);
					if (ret)
						goto blk_out;
				}
			}
		}
	}
#endif

blk_out:
	return ret;
}

static int dynamic_hotplug(enum hotplug_cmd cmd)
{
	int ret = 0;

	mutex_lock(&dm_hotplug_lock);

	switch (cmd) {
	case CMD_LOW_POWER:
		ret = __cpu_hotplug(true, cmd);
		in_low_power_mode = true;
		break;
	case CMD_CORE_ONE_OUT:
	case CMD_BIG_OUT:
		ret = __cpu_hotplug(true, cmd);
		break;
	case CMD_CORE_ONE_IN:
	case CMD_BIG_IN:
		ret = __cpu_hotplug(false, cmd);
		break;
	case CMD_NORMAL:
		ret = __cpu_hotplug(false, cmd);
		in_low_power_mode = false;
		break;
	}

	mutex_unlock(&dm_hotplug_lock);

	return ret;
}

#if defined(CONFIG_SCHED_HMP)
int big_cores_hotplug(bool out_flag)
{
	int ret = 0;

	mutex_lock(&big_hotplug_lock);

	if (out_flag) {
		do_hotplug_out = true;
		if (big_hotpluged) {
			big_hotpluged++;
			do_hotplug_out = false;
			goto out;
		}

		ret = dynamic_hotplug(CMD_BIG_OUT);
		if (!ret) {
			big_hotpluged++;
			do_hotplug_out = false;
		}
	} else {
		if (WARN_ON(!big_hotpluged)) {
			pr_err("%s: big cores already hotplug in\n",
					__func__);
			ret = -EINVAL;
			goto out;
		}

		if (big_hotpluged > 1) {
			big_hotpluged--;
			goto out;
		}

		ret = dynamic_hotplug(CMD_BIG_IN);
		if (!ret)
			big_hotpluged--;
	}

out:
	mutex_unlock(&big_hotplug_lock);

	return ret;
}

static void event_hotplug_in_work(struct work_struct *work)
{
	if (!dynamic_hotplug(CMD_NORMAL))
		prev_cmd = CMD_NORMAL;
	else
		pr_err("%s: failed hotplug in\n", __func__);
}

static DECLARE_WORK(hotplug_in_work, event_hotplug_in_work);

void event_hotplug_in(void)
{
	if (hotplug_wq)
		queue_work(hotplug_wq, &hotplug_in_work);
}
#endif

static int exynos_dm_hotplug_notifier(struct notifier_block *notifier,
					unsigned long pm_event, void *v)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&thread_lock);
		in_suspend_prepared = true;
		if (!dynamic_hotplug(CMD_LOW_POWER))
			prev_cmd = CMD_LOW_POWER;
		exynos_dm_hotplug_disable();
		if (dm_hotplug_task) {
			kthread_stop(dm_hotplug_task);
			dm_hotplug_task = NULL;
		}
		mutex_unlock(&thread_lock);
		break;

	case PM_POST_SUSPEND:
		mutex_lock(&thread_lock);
		exynos_dm_hotplug_enable();

		dm_hotplug_task =
			kthread_create(on_run, NULL, "thread_hotplug");
		if (IS_ERR(dm_hotplug_task)) {
			mutex_unlock(&thread_lock);
			pr_err("Failed in creation of thread.\n");
			return -EINVAL;
		}

		in_suspend_prepared = false;

		wake_up_process(dm_hotplug_task);

		mutex_unlock(&thread_lock);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_dm_hotplug_nb = {
	.notifier_call = exynos_dm_hotplug_notifier,
	.priority = 1,
};

static int exynos_dm_hotplut_reboot_notifier(struct notifier_block *this,
				unsigned long code, void *_cmd)
{
	switch (code) {
	case SYSTEM_POWER_OFF:
	case SYS_RESTART:
		mutex_lock(&thread_lock);
		exynos_dm_hotplug_disable();
		if (dm_hotplug_task) {
			kthread_stop(dm_hotplug_task);
			dm_hotplug_task = NULL;
		}
		mutex_unlock(&thread_lock);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_dm_hotplug_reboot_nb = {
	.notifier_call = exynos_dm_hotplut_reboot_notifier,
};

#if !defined(CONFIG_SCHED_HMP)
static void update_nr_running_count(void)
{
        cur_nr_running = nr_running();

        if (cur_nr_running >= nr_running_threshold) {
                if (nr_running_count < nr_running_range)
                        nr_running_count++;
        } else {
                if (nr_running_count > 0)
                        nr_running_count--;
        }

        if (nr_running_count) {
                if (!core1_in_by_nr_running) {
                        core1_hotplug_in(true);
                        core1_in_by_nr_running = true;
                }
        } else {
                if (core1_in_by_nr_running) {
                        core1_hotplug_in(false);
                        core1_in_by_nr_running = false;
                }
        }
}
#endif

static int low_stay = 0;

static enum hotplug_cmd diagnose_condition(void)
{
	int ret;
	unsigned int normal_min_freq;

#if defined(CONFIG_CPU_FREQ_GOV_INTERACTIVE)
	normal_min_freq = cpufreq_interactive_get_hispeed_freq();
#else
	normal_min_freq = NORMALMIN_FREQ;
#endif

#if !defined(CONFIG_SCHED_HMP)
        update_nr_running_count();
#endif

	ret = CMD_NORMAL;

#if defined( CONFIG_SEC_PM) && defined (CONFIG_SOC_EXYNOS4415)
        normal_min_freq = threshold;
#endif
	if (cur_load_freq > normal_min_freq)
		low_stay = 0;
	else if (cur_load_freq <= normal_min_freq && low_stay <= 5)
		low_stay++;

#if defined(CONFIG_SOC_EXYNOS3250) || defined(CONFIG_SOC_EXYNOS3472)
	if (low_stay > 5)
#else
	if (low_stay > 5 && !lcd_is_on)
#endif
		ret = CMD_LOW_POWER;

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

static int on_run(void *data)
{
	int on_cpu = 0;
	int ret;

	struct cpumask thread_cpumask;

	cpumask_clear(&thread_cpumask);
	cpumask_set_cpu(on_cpu, &thread_cpumask);
	sched_setaffinity(0, &thread_cpumask);

	while (!kthread_should_stop()) {
		calc_load();
		exe_cmd = diagnose_condition();

#ifdef DM_HOTPLUG_DEBUG
		pr_info("frequency info : %d, prev_cmd %d, exe_cmd %d\n",
				cur_load_freq, prev_cmd, exe_cmd);
		pr_info("lcd is on : %d, low power mode = %d, dm_hotplug disable = %d\n",
				lcd_is_on, in_low_power_mode, exynos_dm_hotplug_disabled());
#endif
		if (exynos_dm_hotplug_disabled())
			goto sleep;

		if (prev_cmd != exe_cmd) {
			ret = dynamic_hotplug(exe_cmd);
			if (ret < 0) {
				if (ret == -EBUSY)
					goto sleep;
				else
					goto failed_out;
			}
		}

		prev_cmd = exe_cmd;

sleep:
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout_interruptible(msecs_to_jiffies(delay));
		set_current_state(TASK_RUNNING);
	}

	pr_info("stopped %s\n", dm_hotplug_task->comm);

	return 0;

failed_out:
	panic("%s: failed dynamic hotplug (exe_cmd %d)\n", __func__, exe_cmd);
}

#if defined(CONFIG_SCHED_HMP)
bool is_big_hotpluged(void)
{
	return big_hotpluged ? true : false;
}
#endif

static int __init dm_cpu_hotplug_init(void)
{
	int ret = 0;

	dm_hotplug_task =
		kthread_create(on_run, NULL, "thread_hotplug");
	if (IS_ERR(dm_hotplug_task)) {
		pr_err("Failed in creation of thread.\n");
		return -EINVAL;
	}

	fb_register_client(&fb_block);

#if defined( CONFIG_SEC_PM) && defined (CONFIG_SOC_EXYNOS4415)
	ret = sysfs_create_file(power_kobj, &threshold_dm_hotplug.attr);
	if (ret) {
		pr_err("%s: failed to create threshold_dm_hotplug sysfs interface\n",
			__func__);
                goto err_threshold_dm_hotplug;
	}
#endif

#ifdef CONFIG_PM
	ret = sysfs_create_file(power_kobj, &enable_dm_hotplug.attr);
	if (ret) {
		pr_err("%s: failed to create enable_dm_hotplug sysfs interface\n",
			__func__);
		goto err_enable_dm_hotplug;
	}

#if !defined(CONFIG_SCHED_HMP)
        ret = sysfs_create_file(power_kobj, &core_hotplug_in.attr);
        if (ret) {
                pr_err("%s: failed to create core1_hotplug_in sysfs interface\n",
                        __func__);
                goto err_core1_hotplug_in;
        }

        ret = sysfs_create_file(power_kobj, &hotplug_nr_running.attr);
        if (ret) {
                pr_err("%s: failed to create hotplug_nr_running sysfs interface\n",
                        __func__);
                goto err_hotplug_nr_running;
        }
#endif
#endif

#if defined(CONFIG_SCHED_HMP)
        hotplug_wq = create_singlethread_workqueue("event-hotplug");
        if (!hotplug_wq) {
                ret = -ENOMEM;
                goto err_wq;
        }
#endif

	register_pm_notifier(&exynos_dm_hotplug_nb);
	register_reboot_notifier(&exynos_dm_hotplug_reboot_nb);

	wake_up_process(dm_hotplug_task);

	return ret;

#if defined(CONFIG_SCHED_HMP)
err_wq:
#ifdef CONFIG_PM
	sysfs_remove_file(power_kobj, &enable_dm_hotplug.attr);
#endif
#else
err_hotplug_nr_running:
	sysfs_remove_file(power_kobj, &core_hotplug_in.attr);
err_core1_hotplug_in:
#ifdef CONFIG_PM
	sysfs_remove_file(power_kobj, &enable_dm_hotplug.attr);
#endif
#endif

err_enable_dm_hotplug:
#if defined( CONFIG_SEC_PM) && defined (CONFIG_SOC_EXYNOS4415)
        sysfs_remove_file(power_kobj, &threshold_dm_hotplug.attr);
err_threshold_dm_hotplug:
#endif
	fb_unregister_client(&fb_block);
	kthread_stop(dm_hotplug_task);

	return ret;
}

late_initcall(dm_cpu_hotplug_init);
