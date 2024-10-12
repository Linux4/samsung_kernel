#include <linux/input/input_booster.h>
#include <mt-plat/cpu_ctrl.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
#include "eas_ctrl_plat.h"

#define LITTLE_CPU_FREQ 1138000
#define INPUT_BOOSTER_DDR_OPP_NUM 2

#if defined(CONFIG_MACH_MT6765)
enum cluster{
	BIG = 0,
	LITTLE,
	CLUSTER_NUM
};
#else
enum cluster{
	LITTLE = 0,
	BIG,
	CLUSTER_NUM
};
#endif

int dvfsrc_opp_table[INPUT_BOOSTER_DDR_OPP_NUM] = {
	1866,
	1534
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

struct cpu_ctrl_data freq_to_set[CLUSTER_NUM];

int release_value[MAX_RES_COUNT];

void set_freq_limit(int kicker, int value)
{
	if(value > 0){
		freq_to_set[BIG].min = value;
		freq_to_set[LITTLE].min = LITTLE_CPU_FREQ;
	} else {
		freq_to_set[BIG].min = value;
		freq_to_set[LITTLE].min = value;
	}

	update_userlimit_cpu_freq(kicker, CLUSTER_NUM, freq_to_set);
}

static struct mtk_pm_qos_request ddr_pm_qos_request;

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
				mtk_pm_qos_update_request(&ddr_pm_qos_request, ddr_level);
				pr_booster("%s :: bus value : %ld", __func__, dvfsrc_opp_table[ddr_level]);
			}
			break;
		case SCHEDBOOST:
			set_sched_boost_type(value);
			pr_booster("%s :: schedboost value : %ld", __func__, value);
			break;
		default:
			pr_booster("%s :: res_type : %d is not used", __func__, res_type);
			break;
		}
	}
}

void ib_release_booster(long *rel_flags)
{
	//cpufreq : -1, ddrfreq : 0, HMP : 0, lpm_bias = 0
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
			mtk_pm_qos_update_request(&ddr_pm_qos_request, value);
			pr_booster("%s :: bus value : %ld", __func__, value);
			break;
		case SCHEDBOOST:
			set_sched_boost_type(value);
			pr_booster("%s :: schedboost value : %ld", __func__, value);
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

	mtk_pm_qos_add_request(&ddr_pm_qos_request,  MTK_PM_QOS_DDR_OPP,  MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);

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
	mtk_pm_qos_remove_request(&ddr_pm_qos_request);
}