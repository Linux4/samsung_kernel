#include <v1/exynos-gpu-profiler.h>

/************************************************************************
 *				SUPPORT-PROFILER				*
 ************************************************************************/
u32 gpupro_get_table_cnt(s32 id)
{
	return profiler.table_cnt;
}

u32 gpupro_get_freq_table(s32 id, u32 *table)
{
	int idx;
	for (idx = 0; idx < profiler.table_cnt; idx++)
		table[idx] = profiler.table[idx].freq;

	return idx;
}

u32 gpupro_get_max_freq(s32 id)
{
	return profiler.table[profiler.max_freq_idx].freq;
}

u32 gpupro_get_min_freq(s32 id)
{
	return profiler.table[profiler.min_freq_idx].freq;
}

u32 gpupro_get_freq(s32 id)
{
	return profiler.result[PROFILER].fc_result.freq[CS_ACTIVE];
}

void gpupro_get_power(s32 id, u64 *dyn_power, u64 *st_power)
{
	*dyn_power = profiler.result[PROFILER].fc_result.dyn_power;
	*st_power = profiler.result[PROFILER].fc_result.st_power;
}

void gpupro_get_power_change(s32 id, s32 freq_delta_ratio,
			u32 *freq, u64 *dyn_power, u64 *st_power)
{
	struct gpu_profile_result *result = &profiler.result[PROFILER];
	struct freq_cstate_result *fc_result = &result->fc_result;
	int flag = (STATE_SCALE_WO_SPARE | STATE_SCALE_TIME | STATE_SCALE_WITH_ORG_CAP);

	get_power_change(profiler.table, profiler.table_cnt,
		profiler.cur_freq_idx, profiler.min_freq_idx, profiler.max_freq_idx,
		fc_result->time[CS_ACTIVE], fc_result->time[CLK_OFF], freq_delta_ratio,
		fc_result->profile_time, result->avg_temp, flag, dyn_power, st_power, freq);
}

u32 gpupro_get_active_pct(s32 id)
{
	return profiler.result[PROFILER].fc_result.ratio[CS_ACTIVE];
}

s32 gpupro_get_temp(s32 id)
{
	return profiler.result[PROFILER].avg_temp;
}

void gpupro_set_margin(s32 id, s32 margin)
{
	exynos_profiler_set_freq_margin(id, margin);
	return;
}

static void gpupro_reset_profiler(int user)
{
	struct freq_cstate *fc = &profiler.fc;
	struct freq_cstate_snapshot *fc_snap = &profiler.fc_snap[user];

	profiler.fc.time[CS_ACTIVE] = exynos_profiler_get_time_in_state();
	sync_fcsnap_with_cur(fc, fc_snap, profiler.table_cnt);
	fc_snap->last_snap_time = exynos_profiler_get_tis_last_update();
}

static u32 gpupro_update_profile(int user);
u32 gpupro_update_mode(s32 id, int mode)
{
	if (profiler.enabled != mode) {
		/* reset profiler struct at start time */
		if (mode)
			gpupro_reset_profiler(PROFILER);

		profiler.enabled = mode;

		return 0;
	}
	gpupro_update_profile(PROFILER);

	return 0;
}

void gpupro_get_timeinfo(u64 *time_table)
{
	exynos_profiler_get_run_times(time_table);
}

void gpupro_get_pidinfo(u32 *pid_list, u8 *core_list)
{
	exynos_profiler_get_pid_list(pid_list, core_list);
}

void gpupro_set_targetframetime(int us)
{
	exynos_profiler_set_targetframetime(us);
	return;
}

u64 gpupro_get_gpu_hw_status(void)
{
	return 0;
}

struct private_fn_gpu gpu_pd_fn = {
	.get_timeinfo           = &gpupro_get_timeinfo,
	.get_pidinfo            = &gpupro_get_pidinfo,
	.set_targetframetime    = &gpupro_set_targetframetime,
	.get_gpu_hw_status      = &gpupro_get_gpu_hw_status,
};
struct domain_fn gpu_fn = {
	.get_table_cnt		= &gpupro_get_table_cnt,
	.get_freq_table		= &gpupro_get_freq_table,
	.get_max_freq		= &gpupro_get_max_freq,
	.get_min_freq		= &gpupro_get_min_freq,
	.get_freq			= &gpupro_get_freq,
	.get_power			= &gpupro_get_power,
	.get_power_change	= &gpupro_get_power_change,
	.get_active_pct		= &gpupro_get_active_pct,
	.get_temp			= &gpupro_get_temp,
	.set_margin			= &gpupro_set_margin,
	.update_mode		= &gpupro_update_mode,
};

/************************************************************************
 *			Gathering GPU Freq Information			*
 ************************************************************************/
static u32 gpupro_update_profile(int user)
{
	struct gpu_profile_result *result = &profiler.result[user];
	struct freq_cstate *fc = &profiler.fc;
	struct freq_cstate_snapshot *fc_snap = &profiler.fc_snap[user];
	struct freq_cstate_result *fc_result = &result->fc_result;
	ktime_t tis_last_update;
	ktime_t last_snap_time;

	/* Common Data */
	if (profiler.tz) {
		int temp = exynos_profiler_get_temp(profiler.tz);
		profiler.result[user].avg_temp = (temp + profiler.result[user].cur_temp) >> 1;
		profiler.result[user].cur_temp = temp;
	}

	profiler.cur_freq_idx = get_idx_from_freq(profiler.table,
			profiler.table_cnt, exynos_profiler_get_cur_clock(), RELATION_LOW);
	profiler.max_freq_idx = get_idx_from_freq(profiler.table,
			profiler.table_cnt, exynos_profiler_get_max_locked_freq(), RELATION_LOW);
	profiler.min_freq_idx = get_idx_from_freq(profiler.table,
			profiler.table_cnt, exynos_profiler_get_min_locked_freq(), RELATION_HIGH);

	profiler.fc.time[CS_ACTIVE] = exynos_profiler_get_time_in_state();

	tis_last_update = exynos_profiler_get_tis_last_update();
	last_snap_time = fc_snap->last_snap_time;
	make_snapshot_and_time_delta(fc, fc_snap, fc_result, profiler.table_cnt);
#if IS_ENABLED(CONFIG_DRM_SGPU)
	fc_result->profile_time = tis_last_update - last_snap_time;
	fc_snap->last_snap_time = tis_last_update;
#endif

	compute_freq_cstate_result(profiler.table, fc_result, profiler.table_cnt,
			profiler.cur_freq_idx, profiler.result[user].avg_temp);
	return 0;
}

/************************************************************************
 *						INITIALIZATON									*
 ************************************************************************/
static int register_export_fn(u32 *max_freq, u32 *min_freq, u32 *cur_freq)
{
	*max_freq = exynos_profiler_get_max_locked_freq();
	*min_freq = exynos_profiler_get_min_locked_freq();
	*cur_freq = exynos_profiler_get_cur_clock();

	profiler.table_cnt = exynos_profiler_get_step();
	profiler.fc.time[CS_ACTIVE] = exynos_profiler_get_time_in_state();

	return 0;
}

static int parse_dt(struct device_node *dn)
{
	int ret;

	/* necessary data */
	ret = of_property_read_u32(dn, "cal-id", &profiler.cal_id);
	if (ret)
		return -2;

	/* un-necessary data */
	ret = of_property_read_s32(dn, "profiler-id", &profiler.profiler_id);
	if (ret)
		profiler.profiler_id = -1;	/* Don't support profiler */

	of_property_read_u32(dn, "power-coefficient", &profiler.dyn_pwr_coeff);
	of_property_read_u32(dn, "static-power-coefficient", &profiler.st_pwr_coeff);
	of_property_read_string(dn, "tz-name", &profiler.tz_name);

	return 0;
}

static int init_profile_result(struct gpu_profile_result *result, int size)
{
	if (init_freq_cstate_result(&result->fc_result, size))
		return -ENOMEM;
	return 0;
}

static void show_profiler_info(void)
{
	int idx;

	pr_info("================ gpu domain ================\n");
	if (profiler.table) {
		pr_info("min= %dKHz, max= %dKHz\n",
				profiler.table[profiler.table_cnt - 1].freq, profiler.table[0].freq);
		for (idx = 0; idx < profiler.table_cnt; idx++)
			pr_info("lv=%3d freq=%8d volt=%8d dyn_cost=%lld st_cost=%lld\n",
				idx, profiler.table[idx].freq, profiler.table[idx].volt,
				profiler.table[idx].dyn_cost,
				profiler.table[idx].st_cost);
	}
	if (profiler.tz_name)
		pr_info("support temperature (tz_name=%s)\n", profiler.tz_name);
	if (profiler.profiler_id != -1)
		pr_info("support profiler domain(id=%d)\n", profiler.profiler_id);
}

static int exynos_gpu_profiler_probe(struct platform_device *pdev)
{
	unsigned int org_max_freq, org_min_freq, cur_freq;
	int ret, idx;

	/* get node of device tree */
	if (!pdev->dev.of_node) {
		pr_err("gpupro: failed to get device treee\n");
		return -EINVAL;
	}
	profiler.root = pdev->dev.of_node;

	/* Parse data from Device Tree to init domain */
	ret = parse_dt(profiler.root);
	if (ret) {
		pr_err("gpupro: failed to parse dt(ret: %d)\n", ret);
		return -EINVAL;
	}

	register_export_fn(&org_max_freq, &org_min_freq, &cur_freq);

	/* init freq table */
	profiler.table = init_fvtable(
			(u32 *)exynos_profiler_get_freq_table(), profiler.table_cnt,
			profiler.cal_id, org_max_freq, org_min_freq,
			profiler.dyn_pwr_coeff, profiler.st_pwr_coeff,
			PWR_COST_CFVV, PWR_COST_CFVV);
	if (!profiler.table) {
		pr_err("gpupro: failed to init freq_table\n");
		return -EINVAL;
	}

	ret = exynos_profiler_pb_set_powertable(profiler.profiler_id, profiler.table_cnt, profiler.table);
	if (ret) {
		pr_err("gpupro: failed to set power table(ret: %d)\n", ret);
		return -ENOMEM;
	}

	profiler.max_freq_idx = 0;
	profiler.min_freq_idx = profiler.table_cnt - 1;
	profiler.cur_freq_idx = get_idx_from_freq(profiler.table,
				profiler.table_cnt, cur_freq, RELATION_HIGH);

	if (init_freq_cstate(&profiler.fc, profiler.table_cnt))
			return -ENOMEM;

	/* init snapshot & result table */
	for (idx = 0; idx < NUM_OF_USER; idx++) {
		if (init_freq_cstate_snapshot(&profiler.fc_snap[idx],
						profiler.table_cnt))
			return -ENOMEM;

		if (init_profile_result(&profiler.result[idx], profiler.table_cnt))
			return -EINVAL;
	}

	/* get thermal-zone to get temperature */
	if (profiler.tz_name)
		profiler.tz = exynos_profiler_init_temp(profiler.tz_name);

	if (profiler.tz)
		init_static_cost(profiler.table, profiler.table_cnt,
				1, profiler.root, profiler.tz);

	ret = exynos_profiler_register_domain(PROFILER_GPU, &gpu_fn, &gpu_pd_fn);

	show_profiler_info();

	return ret;
}

static const struct of_device_id exynos_gpu_profiler_match[] = {
	{
		.compatible	= "samsung,exynos-gpu-profiler",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_gpu_profiler_match);

static struct platform_driver exynos_gpu_profiler_driver = {
	.probe		= exynos_gpu_profiler_probe,
	.driver	= {
		.name	= "exynos-gpu-profiler",
		.owner	= THIS_MODULE,
		.of_match_table = exynos_gpu_profiler_match,
	},
};

static int exynos_gpu_profiler_init(void)
{
	return platform_driver_register(&exynos_gpu_profiler_driver);
}
late_initcall(exynos_gpu_profiler_init);

MODULE_DESCRIPTION("Exynos GPU Profiler v1");
MODULE_SOFTDEP("pre: exynos_thermal_v2 exynos-cpufreq sgpu exynos_devfreq");
MODULE_LICENSE("GPL");
