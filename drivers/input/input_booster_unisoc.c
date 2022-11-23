#include <linux/input/input_booster.h>
#include <linux/pm_qos.h>

#define LITTLE_CPU_FREQ 1040000
#define INPUT_BOOSTER_DDR_OPP_NUM 8

extern int scene_dfs_request(char *scenario);
extern int scene_exit(char *scenario);
extern int change_scene_freq(char *scenario, unsigned int freq);

enum cluster {
	LITTLE = 0,
	BIG,
	CLUSTER_NUM
};

int dvfsrc_opp_table[INPUT_BOOSTER_DDR_OPP_NUM] = {
	1600,
	1333,
	1024,
	768,
	512,
	384,
	256
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

int release_value[MAX_RES_COUNT];

static struct pm_qos_request cpufreq_little_min_qos;
static struct pm_qos_request cpufreq_big_min_qos;
bool ddr_flag = false;
void ib_set_booster(long* qos_values)
{
	int value = -1;
	int ddr_level = 0;
	int res_type =0;
	int ret = 0;
	for(res_type = 0; res_type < MAX_RES_COUNT; res_type++) {
		value = qos_values[res_type];

		if (value <= 0)
			continue;

		switch(res_type) {
		case CPUFREQ:
			pm_qos_update_request(&cpufreq_big_min_qos, value);
			pm_qos_update_request(&cpufreq_little_min_qos, LITTLE_CPU_FREQ);
			pr_booster("%s :: cpufreq value : %d", __func__, value);
			break;
		case DDRFREQ:
			if (!ddr_flag) {
				scene_dfs_request("inputboost");
				ddr_flag = true;
			}
			ddr_level = trans_freq_to_level(value);
			if (ddr_level != -1) {
				ret = change_scene_freq("inputboost", dvfsrc_opp_table[ddr_level]);
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
			pm_qos_update_request(&cpufreq_big_min_qos, PM_QOS_FREQ_MIN_DEFAULT_VALUE);
			pm_qos_update_request(&cpufreq_little_min_qos, PM_QOS_FREQ_MIN_DEFAULT_VALUE);
			pr_booster("%s :: cpufreq value : %d", __func__, value);
			break;
		case DDRFREQ:
			scene_exit("inputboost");
			ddr_flag = false;
			pr_booster("%s :: bus value : %ld", __func__, value);
			break;
		default:
			pr_booster("%s :: res_type : %d is not used", __func__, res_type);
			break;
		}
	}
}

int input_booster_init_vendor(void)
{
	pm_qos_add_request(&cpufreq_little_min_qos,  PM_QOS_CLUSTER0_FREQ_MIN,  PM_QOS_FREQ_MIN_DEFAULT_VALUE);
	pm_qos_add_request(&cpufreq_big_min_qos,  PM_QOS_CLUSTER1_FREQ_MIN,  PM_QOS_FREQ_MIN_DEFAULT_VALUE);

	return 1;
}

void input_booster_exit_vendor()
{
	pm_qos_remove_request(&cpufreq_little_min_qos);
	pm_qos_remove_request(&cpufreq_big_min_qos);
}
