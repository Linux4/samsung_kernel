#include <linux/input/input_booster.h>
#include <mt-plat/cpu_ctrl.h>
#include <linux/pm_qos.h>

#define LITTLE_CPU_FREQ 1032000
#define INPUT_BOOSTER_DDR_OPP_NUM 8

enum cluster {
	LITTLE = 0,
	BIG,
	CLUSTER_NUM
};

int dvfsrc_opp_table[INPUT_BOOSTER_DDR_OPP_NUM] = {
	4266,
	3200,
	2400,
	1866,
	1600,
	1200,
	800,
	400
};

int trans_freq_to_level(long request_ddr_freq)
{
	int i = 0;

	if (request_ddr_freq <= 0) {
		return -1;
	}

	for (i = 1; i < INPUT_BOOSTER_DDR_OPP_NUM; i++) {
		if (request_ddr_freq > dvfsrc_opp_table[i]) {
			return (i-1);
		}
	}

	return INPUT_BOOSTER_DDR_OPP_NUM-1;
}

struct ppm_limit_data freq_to_set[CLUSTER_NUM];

int release_value[MAX_RES_COUNT];

void set_freq_limit(int kicker, int value)
{
	if (value > 0) {
		freq_to_set[LITTLE].min = LITTLE_CPU_FREQ;
		freq_to_set[BIG].min = value;
	} else {
		freq_to_set[LITTLE].min = value;
		freq_to_set[BIG].min = value;
	}

	update_userlimit_cpu_freq(kicker, CLUSTER_NUM, freq_to_set);
}

static struct pm_qos_request ddr_pm_qos_request;

void ib_set_booster(int* qos_values)
{
	int value = -1;
	int ddr_level = 0;
	int res_type =0;

	for(res_type = 0; res_type < MAX_RES_COUNT; res_type++) {
		value = qos_values[res_type];

		if (value <= 0)
			continue;

		switch(res_type) {
		case CPUFREQ:
			set_freq_limit(CPU_KIR_INPUT_BOOSTER, value);
			pr_booster("%s :: cpufreq value : %d", __func__, value);
			break;
		case DDRFREQ:
			ddr_level = trans_freq_to_level(value);
			if (ddr_level != -1) {
				pm_qos_update_request(&ddr_pm_qos_request, ddr_level);
				pr_booster("%s :: bus value : %ld", __func__, dvfsrc_opp_table[ddr_level]);
			}
			break;
		default:
			pr_booster("%s :: res_type : %d is not used", __func__, res_type);
			break;
		}
	}
}

void ib_release_booster(long *rel_flags)
{
	//cpufreq : -1, ddrfreq : 0
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
			set_freq_limit(CPU_KIR_INPUT_BOOSTER, value);
			pr_booster("%s :: cpufreq value : %d", __func__, value);
			break;
		case DDRFREQ:
			pm_qos_update_request(&ddr_pm_qos_request, value);
			pr_booster("%s :: bus value : %ld", __func__, value);
			break;
		default:
			pr_booster("%s :: res_type : %d is not used", __func__, res_type);
			break;
		}
	}
}

void input_booster_init_vendor(int* release_val)
{
	int i = 0;

	pm_qos_add_request(&ddr_pm_qos_request,  PM_QOS_DDR_OPP,  PM_QOS_DDR_OPP_DEFAULT_VALUE);

	for (i = 0; i < CLUSTER_NUM; i++) {
		freq_to_set[i].max = -1; /* -1 means don't care */
		freq_to_set[i].min = -1;
	}
	for (i = 0; i < MAX_RES_COUNT; i++) {
		release_value[i] = release_val[i];
	}
}

void input_booster_exit_vendor()
{
	pm_qos_remove_request(&ddr_pm_qos_request);
}