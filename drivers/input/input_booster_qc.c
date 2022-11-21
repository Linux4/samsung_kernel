#include <linux/input/input_booster.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/syscalls.h>

int release_value[MAX_RES_COUNT];
int current_hmp_boost;
void set_hmp(int level);

#define LITTLE_CPU_FREQ 1171200
#define LITTLE_POLICY 4

struct cpu_sync {
	int cpu;
	unsigned int input_boost_min;
};

static DEFINE_PER_CPU(struct cpu_sync, sync_info);

static void update_policy_online(void)
{
	unsigned int i;

	/* Re-evaluate policy to trigger adjust notifier for online CPUs */

	get_online_cpus();
	for_each_online_cpu(i) {
		pr_debug("Updating policy for CPU%d\n", i);
		cpufreq_update_policy(i);
	}
	put_online_cpus();
}

void set_cpu_freq_limit(int value) {
	struct cpu_sync *i_sync_info;
	unsigned int i;

	if (value > 0) {
#if defined(CONFIG_ARCH_SDM450)
		for_each_possible_cpu(i) {
			i_sync_info = &per_cpu(sync_info, i);
			i_sync_info->input_boost_min = value;
		}
#elif(CONFIG_ARCH_SDM439)
		for_each_possible_cpu(i) {
			i_sync_info = &per_cpu(sync_info, i);
			if(i < LITTLE_POLICY)
				i_sync_info->input_boost_min = value;
			else
				i_sync_info->input_boost_min = LITTLE_CPU_FREQ;
		}
#endif
	}else {
		for_each_possible_cpu(i) {
			i_sync_info = &per_cpu(sync_info, i);
			i_sync_info->input_boost_min = 0;
		}
	}


	update_policy_online();
}

static int boost_adjust_notify(struct notifier_block *nb, unsigned long val,
							void *data)
{
	struct cpufreq_policy *policy = data;
	unsigned int cpu = policy->cpu;
	struct cpu_sync *s = &per_cpu(sync_info, cpu);
	unsigned int ib_min = s->input_boost_min;

	switch (val) {
	case CPUFREQ_ADJUST:
		if (!ib_min)
			break;

		pr_debug("CPU%u policy min before boost: %u kHz\n",
			 cpu, policy->min);
		pr_debug("CPU%u boost min: %u kHz\n", cpu, ib_min);

		cpufreq_verify_within_limits(policy, ib_min, UINT_MAX);

		pr_debug("CPU%u policy min after boost: %u kHz\n",
			 cpu, policy->min);
		break;
	}

	return NOTIFY_OK;
}
static struct notifier_block boost_adjust_nb = {
	.notifier_call = boost_adjust_notify,
};

void set_hmp(int level)
{
	if (level != current_hmp_boost) {
		if (level == 0) {
			level = -current_hmp_boost;
			current_hmp_boost = 0;
		} else {
			current_hmp_boost = level;
		}
		pr_booster("[Input Booster2] ******     hmp_boost : %d ( %s )\n", level, __func__);
		if (sched_set_boost(level) < 0)
			pr_err("[Input Booster2] ******            !!! fail to HMP !!!\n");
	}
}

void ib_set_booster(int *qos_values)
{
	int value = -1;
	int ddr_idx = 0;
	int res_type =0;

	for(res_type = 0; res_type < MAX_RES_COUNT; res_type++) {
		value = qos_values[res_type];

		if (value <= 0)
			continue;

		switch(res_type) {
		case CPUFREQ:
			set_cpu_freq_limit(value);
			pr_booster("%s :: cpufreq value : %d", __func__, value);
			break;
		case HMPBOOST:
			set_hmp(value);
			pr_booster("%s :: hmpboost value : %ld", __func__, value);
			break;
		default:
			pr_booster("%s :: res_type : %d is not used", __func__, res_type);
			break;
		}
	}
}

void ib_release_booster(long *rel_flags)
{
	int flag;
	int value;
	int res_type = 0;

	for (res_type = 0; res_type < MAX_RES_COUNT; res_type++) {
		flag = rel_flags[res_type];
		if (flag <= 0)
			continue;

		value = release_value[res_type];

		switch(res_type) {
		case CPUFREQ:
			set_cpu_freq_limit(value);
			pr_booster("%s :: cpufreq value : %d", __func__, value);
			break;
		case HMPBOOST:
			set_hmp(value);
			break;
		default:
			pr_booster("%s :: res_type : %d is not used", __func__, res_type);
			break;
		}
	}
}

void input_booster_init_vendor(int* release_val)
{
	int cpu, i;
	struct cpu_sync *s;
	current_hmp_boost = 0;

	for_each_possible_cpu(cpu) {
		s = &per_cpu(sync_info, cpu);

		s->cpu = cpu;
	}

	for (i = 0; i < MAX_RES_COUNT; i++) {
		release_value[i] = release_val[i];
	}

	cpufreq_register_notifier(&boost_adjust_nb, CPUFREQ_POLICY_NOTIFIER);
}

void input_booster_exit_vendor(void)
{
	cpufreq_unregister_notifier(&boost_adjust_nb, CPUFREQ_POLICY_NOTIFIER);
}
