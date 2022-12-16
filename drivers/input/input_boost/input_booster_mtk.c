#include <linux/input/input_booster.h>
#include <sched_ctl.h>
#define PM_QOS_DDR_OPP_DEFAULT 16
#define DDR_OPP_NUM 3

#ifndef CONFIG_CPU_FREQ_LIMIT
#define DVFS_TOUCH_ID	0
int set_freq_limit(unsigned long id, unsigned int freq)
{
	pr_err("%s is not yet implemented\n", __func__);
	return 0;
}
#else
	/* TEMP: for KPI */
#define DVFS_TOUCH_ID 1
#endif

int dvfsrc_opp_table[DDR_OPP_NUM] = {
	3600,
	2400,
	1534
};

int trans_freq_to_level(long request_ddr_freq)
{
	int i = 0;

	if (request_ddr_freq <= 0) {
		return -1;
	}

	for (i = 1; i < DDR_OPP_NUM; i++) {
		if (request_ddr_freq > dvfsrc_opp_table[i]) {
			return (i-1);
		}
	}

	return DDR_OPP_NUM-1;
}

static struct pm_qos_request ddr_pm_qos_request;

void ib_set_booster(long *qos_values)
{
	long value = -1;
	int res_type = 0;
	int ddr_level = 0;
	int cur_res_idx;
	int rc = 0;

	for (res_type = 0; res_type < allowed_res_count; res_type++) {

		cur_res_idx = allowed_resources[res_type];
		value = qos_values[cur_res_idx];

		if (value <= 0 && cur_res_idx != SCHEDBOOST)
			continue;

		switch (cur_res_idx) {
		case CPUFREQ:
			set_freq_limit(DVFS_TOUCH_ID, value);
			pr_booster("%s :: cpufreq value : %ld", __func__, value);
			break;
		case DDRFREQ:
			ddr_level = trans_freq_to_level(value);
			if (ddr_level != -1) {
				pm_qos_update_request(&ddr_pm_qos_request, ddr_level);
				pr_booster("%s :: bus value : %ld", __func__, dvfsrc_opp_table[ddr_level]);
			}
			break;
		default:
			pr_booster("%s :: cur_res_idx : %d is not used", __func__, cur_res_idx);
			break;
		}
	}
}

void ib_release_booster(long *rel_flags)
{
	int flag;
	int rc = 0;
	int value;

	int res_type = 0;
	int cur_res_idx;

	for (res_type = 0; res_type < allowed_res_count; res_type++) {

		cur_res_idx = allowed_resources[res_type];
		flag = rel_flags[cur_res_idx];
		if (flag <= 0)
			continue;

		value = release_val[cur_res_idx];

		switch (cur_res_idx) {
		case CPUFREQ:
			set_freq_limit(DVFS_TOUCH_ID, value);
			pr_booster("%s :: cpufreq value : %ld", __func__, value);
			break;
		case DDRFREQ:
			pm_qos_update_request(&ddr_pm_qos_request, value);
			pr_booster("%s :: bus value : %ld", __func__, value);
			break;
		default:
			pr_booster("%s :: cur_res_idx : %d is not used", __func__, cur_res_idx);
			break;
		}
	}
}

int input_booster_init_vendor(void)
{
	pm_qos_add_request(&ddr_pm_qos_request, PM_QOS_DDR_OPP, PM_QOS_DDR_OPP_DEFAULT);
	return 1;
}

void input_booster_exit_vendor(void)
{
	pm_qos_remove_request(&ddr_pm_qos_request);
}