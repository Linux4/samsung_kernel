#include <linux/input/input_booster.h>
#include <vcorefs_v3/mtk_vcorefs_manager.h>
#define KIR_SYSF 13
#define DDR_MAX 999

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

static struct pm_qos_request ddr_pm_qos_request;

void ib_set_booster(long *qos_values)
{
	long value = -1;
	int res_type = 0;
	int cur_res_idx;
	int rc = 0;

	for (res_type = 0; res_type < allowed_res_count; res_type++) {

		cur_res_idx = allowed_resources[res_type];
		value = qos_values[cur_res_idx];

		if (value <= 0)
			continue;

		switch (cur_res_idx) {
		case CPUFREQ:
			set_freq_limit(DVFS_TOUCH_ID, value);
			pr_booster("%s :: cpufreq value : %ld", __func__, value);
			break;
		case DDRFREQ:
			if (value == DDR_MAX)
				value = 0;
			vcorefs_request_dvfs_opp(KIR_SYSF, value);
			pr_booster("%s :: bus value : %ld", __func__, value);
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
			vcorefs_request_dvfs_opp(KIR_SYSF, value);
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
	return 1;
}

void input_booster_exit_vendor(void)
{
	return;
}