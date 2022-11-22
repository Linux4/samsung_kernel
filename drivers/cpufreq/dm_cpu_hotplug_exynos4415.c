#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/mutex.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/suspend.h>

#define NORMALMIN_FREQ	100000
#define POLLING_MSEC	100

static DEFINE_MUTEX(dm_hotplug_lock);

static unsigned int cur_load_freq;
static bool lcd_is_on;

enum hotplug_mode {
	CHP_NORMAL,
	CHP_LOW_POWER,
};

static enum hotplug_mode prev_mode;
static bool exynos_dm_hotplug_disable;

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

	mutex_lock(&dm_hotplug_lock);
	if (exynos_dm_hotplug_disable ||
			cpumask_weight(be_out_cpus) >= NR_CPUS) {
		ret = -EPERM;
		goto out;
	}

	for (i = 1; i < NR_CPUS; i++) {
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
	mutex_unlock(&dm_hotplug_lock);

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
		for (i = 1; i < NR_CPUS; i++)
			cpumask_set_cpu(i, &out_target);
		ret = __cpu_hotplug(&out_target);
		break;
	case CHP_NORMAL:
	default:
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
		mutex_lock(&dm_hotplug_lock);
		exynos_dm_hotplug_disable = true;
		mutex_unlock(&dm_hotplug_lock);
		break;

	case PM_POST_SUSPEND:
		mutex_lock(&dm_hotplug_lock);
		exynos_dm_hotplug_disable = false;
		mutex_unlock(&dm_hotplug_lock);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_dm_hotplug_nb = {
	.notifier_call = exynos_dm_hotplug_notifier,
	.priority = 1,
};

static int low_stay;

static enum hotplug_mode diagnose_condition(void)
{
	int ret;

	ret = CHP_NORMAL;

	if (cur_load_freq > NORMALMIN_FREQ)
		low_stay = 0;
	else if (cur_load_freq <= NORMALMIN_FREQ && low_stay <= 5)
		low_stay++;

	if ((low_stay > 5) && !lcd_is_on)
		ret = CHP_LOW_POWER;

	return ret;
}

static void calc_load(void)
{
	struct cpufreq_policy *policy;
	int cpu = 0;

	policy = cpufreq_cpu_get(cpu);

	if (!policy) {
		pr_err("Invalid policy\n");
		return;
	}

	cur_load_freq = 0;
	if (policy->cur > cur_load_freq)
		cur_load_freq = policy->cur;

	cpufreq_cpu_put(policy);
	return;
}

static int thread_run_flag;

static int on_run(void *data)
{
	int on_cpu = 0;
	enum hotplug_mode exe_mode;

	struct cpumask thread_cpumask;

	cpumask_clear(&thread_cpumask);
	cpumask_set_cpu(on_cpu, &thread_cpumask);
	sched_setaffinity(0, &thread_cpumask);

	prev_mode = CHP_NORMAL;
	thread_run_flag = 1;

	while (thread_run_flag) {
		calc_load();
		exe_mode = diagnose_condition();

		if (exe_mode != prev_mode) {
#ifdef DM_HOTPLUG_DEBUG
			pr_debug("frequency info : %d, %s\n", cur_load_freq
				, (exe_mode < 1) ? "NORMAL" : ((exe_mode < 2) ? "LOW" : "HIGH"));
#endif
			if (dynamic_hotplug(exe_mode) < 0)
				exe_mode = prev_mode;
		}

		prev_mode = exe_mode;
		msleep(POLLING_MSEC);
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

	exynos_dm_hotplug_disable = false;

	register_pm_notifier(&exynos_dm_hotplug_nb);
}
