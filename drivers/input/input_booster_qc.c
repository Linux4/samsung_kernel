#include <linux/input/input_booster.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/syscalls.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>

int current_hmp_boost;
void set_hmp(int level);
struct pm_qos_request lpm_bias_pm_qos_request;
int release_value[MAX_RES_COUNT];

#define MHZ_TO_BPS(mhz, w) ((uint64_t)mhz * 1000 * 1000 * w)
#define MHZ_TO_KBPS(mhz, w) ((uint64_t)mhz * 1000 * w)
#define INPUT_BOOSTER_DDR_OPP_NUM 12
#define LITTLE_CPU_FREQ 1017600
#define BIG_POLICY 4

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
		for_each_possible_cpu(i) {
			i_sync_info = &per_cpu(sync_info, i);
			if(i < BIG_POLICY)
				i_sync_info->input_boost_min = LITTLE_CPU_FREQ;
			else
				i_sync_info->input_boost_min = value;
		}
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

#if IS_ENABLED(CONFIG_ARCH_BENGAL)
#define NUM_BUS_TABLE 11
#define BUS_W 4	/* SM6115 DDR Voting('w' for DDR is 4) */
int ab_ib_bus_vectors[NUM_BUS_TABLE][2] = {
	//{ab, ib}
	{0, 0},		/* 0 */
	{0, 400},	/* 1 */
	{0, 600},	/* 2 */
	{0, 902},	/* 3 */
	{0, 1094},	/* 4 */
	{0, 1362},	/* 5 */
	{0, 1536},	/* 6 */
	{0, 2034},	/* 7 */
	{0, 2706},	/* 8 */
	{0, 3110},	/* 9 */
	{0, 3608},	/* 10 */
};
#else
#define NUM_BUS_TABLE 1
#define BUS_W 0

int ab_ib_bus_vectors[NUM_BUS_TABLE][2] = {
	{0, 0},		/* 0 */
};
#endif	//set null for other chipset

static struct msm_bus_vectors touch_reg_bus_vectors[NUM_BUS_TABLE];

static void fill_bus_vector(void)
{
	int i = 0;

	for (i = 0; i < NUM_BUS_TABLE; i++) {
		touch_reg_bus_vectors[i].src = MSM_BUS_MASTER_AMPSS_M0;
		touch_reg_bus_vectors[i].dst = MSM_BUS_SLAVE_EBI_CH0;
		touch_reg_bus_vectors[i].ab = ab_ib_bus_vectors[i][0];
		touch_reg_bus_vectors[i].ib = MHZ_TO_BPS(ab_ib_bus_vectors[i][1], BUS_W);
	}
}

static struct msm_bus_paths touch_reg_bus_usecases[ARRAY_SIZE(touch_reg_bus_vectors)];
static struct msm_bus_scale_pdata touch_reg_bus_scale_table = {
    .usecase = touch_reg_bus_usecases,
    .num_usecases = ARRAY_SIZE(touch_reg_bus_usecases),
    .name = "touch_bw",
};
static u32 bus_hdl;

int trans_freq_to_idx(long request_ddr_freq)
{
	int i = 0;

	if (request_ddr_freq <= 0) {
		return 0;
	}

	// in case of null table, return 0
	if (NUM_BUS_TABLE == 1) {
		return 0;
	}

	for (i = 0; i < NUM_BUS_TABLE-1; i++) {
		if (request_ddr_freq > ab_ib_bus_vectors[i][1] &&
			request_ddr_freq <= ab_ib_bus_vectors[i+1][1]) {
			return (i+1);
		}
	}

	return i+1;
}


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
		case DDRFREQ:
			ddr_idx = trans_freq_to_idx(value);
			pr_booster("%s :: bus value : %ld", __func__, value);
			msm_bus_scale_client_update_request(bus_hdl, ddr_idx);
			break;
		case HMPBOOST:
			set_hmp(value);
			pr_booster("%s :: hmpboost value : %ld", __func__, value);
			break;
		case LPMBIAS:
			pm_qos_update_request(&lpm_bias_pm_qos_request, value);
			pr_booster("%s :: LPM Bias value : %ld", __func__, value);
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
	int ddr_idx = 0;
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
		case DDRFREQ:
			ddr_idx = trans_freq_to_idx(value);
			msm_bus_scale_client_update_request(bus_hdl, ddr_idx);
			pr_booster("%s :: bus value : %ld", __func__, value);
			break;
		case HMPBOOST:
			set_hmp(value);
			break;
		case LPMBIAS:
			pm_qos_update_request(&lpm_bias_pm_qos_request, value);
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

	fill_bus_vector();

	for (i = 0; i < touch_reg_bus_scale_table.num_usecases; i++) {
		touch_reg_bus_usecases[i].num_paths = 1;
		touch_reg_bus_usecases[i].vectors = &touch_reg_bus_vectors[i];
	}

	bus_hdl = msm_bus_scale_register_client(&touch_reg_bus_scale_table);

	pm_qos_add_request(&lpm_bias_pm_qos_request,
		PM_QOS_BIAS_HYST, PM_QOS_BIAS_HYST_DEFAULT_VALUE);

	for (i = 0; i < MAX_RES_COUNT; i++) {
		release_value[i] = release_val[i];
	}

	cpufreq_register_notifier(&boost_adjust_nb, CPUFREQ_POLICY_NOTIFIER);
}

void input_booster_exit_vendor(void)
{
	pm_qos_remove_request(&lpm_bias_pm_qos_request);
}
