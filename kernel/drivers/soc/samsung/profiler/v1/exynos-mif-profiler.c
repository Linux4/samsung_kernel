#include <v1/exynos-mif-profiler.h>

/************************************************************************
 *				HELPER					*
 ************************************************************************/

static inline void calc_delta(u64 *result_table, u64 *prev_table, u64 *cur_table, int size)
{
	int i;
	u64 delta, cur;

	for (i = 0; i < size; i++) {
		cur = cur_table[i];
		delta = cur - prev_table[i];
		result_table[i] = delta;
		prev_table[i] = cur;
	}
}

/************************************************************************
 *				SUPPORT-PROFILER				*
 ************************************************************************/
u32 mifpro_get_table_cnt(s32 id)
{
	return profiler.table_cnt;
}

u32 mifpro_get_freq_table(s32 id, u32 *table)
{
	int idx;
	for (idx = 0; idx < profiler.table_cnt; idx++)
		table[idx] = profiler.table[idx].freq;

	return idx;
}

u32 mifpro_get_max_freq(s32 id)
{
	return exynos_pm_qos_request(profiler.freq_infos.pm_qos_class_max);
}

u32 mifpro_get_min_freq(s32 id)
{
	return exynos_pm_qos_request(profiler.freq_infos.pm_qos_class);
}

u32 mifpro_get_freq(s32 id)
{
	return profiler.result[PROFILER].fc_result.freq[CS_ACTIVE];
}

void mifpro_get_power(s32 id, u64 *dyn_power, u64 *st_power)
{
	*dyn_power = profiler.result[PROFILER].fc_result.dyn_power;
	*st_power = profiler.result[PROFILER].fc_result.st_power;
}

void mifpro_get_power_change(s32 id, s32 freq_delta_ratio,
			u32 *freq, u64 *dyn_power, u64 *st_power)
{
	struct mif_profile_result *result = &profiler.result[PROFILER];
	struct freq_cstate_result *fc_result = &result->fc_result;
	int flag = (STATE_SCALE_WO_SPARE | STATE_SCALE_CNT);
	u64 dyn_power_backup;

	get_power_change(profiler.table, profiler.table_cnt,
		profiler.cur_freq_idx, profiler.min_freq_idx, profiler.max_freq_idx,
		result->tdata_in_state, fc_result->time[CLK_OFF], freq_delta_ratio,
		fc_result->profile_time, result->avg_temp, flag, dyn_power, st_power, freq);

	dyn_power_backup = *dyn_power;

	get_power_change(profiler.table, profiler.table_cnt,
		profiler.cur_freq_idx, profiler.min_freq_idx, profiler.max_freq_idx,
		fc_result->time[CS_ACTIVE], fc_result->time[CLK_OFF], freq_delta_ratio,
		fc_result->profile_time, result->avg_temp, flag, dyn_power, st_power, freq);

	*dyn_power = dyn_power_backup;
}

u32 mifpro_get_active_pct(s32 id)
{
	return profiler.result[PROFILER].fc_result.ratio[CS_ACTIVE];
}

s32 mifpro_get_temp(s32 id)
{
	return profiler.result[PROFILER].avg_temp;
}
void mifpro_set_margin(s32 id, s32 margin)
{
	return;
}

u32 mifpro_update_profile(int user);
u32 mifpro_update_mode(s32 id, int mode)
{
	int i;

	if (profiler.enabled == 0 && mode == 1) {
		struct freq_cstate *fc = &profiler.fc;
		struct freq_cstate_snapshot *fc_snap = &profiler.fc_snap[PROFILER];

		sync_fcsnap_with_cur(fc, fc_snap, profiler.table_cnt);

		profiler.enabled = mode;

		exynos_devfreq_set_profile(profiler.devfreq_type, 1);
	}
	else if (profiler.enabled == 1 && mode == 0) {
		exynos_devfreq_set_profile(profiler.devfreq_type, 0);
		profiler.enabled = mode;

		// clear
		for (i = 0; i < NUM_OF_CSTATE; i++) {
			memset(profiler.result[PROFILER].fc_result.time[i], 0, sizeof(ktime_t) * profiler.table_cnt);
			profiler.result[PROFILER].fc_result.ratio[i] = 0;
			profiler.result[PROFILER].fc_result.freq[i] = 0;
			memset(profiler.fc.time[i], 0, sizeof(ktime_t) * profiler.table_cnt);
			memset(profiler.fc_snap[PROFILER].time[i], 0, sizeof(ktime_t) * profiler.table_cnt);
		}
		profiler.result[PROFILER].fc_result.dyn_power = 0;
		profiler.result[PROFILER].fc_result.st_power = 0;
		profiler.result[PROFILER].fc_result.profile_time = 0;
		profiler.fc_snap[PROFILER].last_snap_time = 0;
		memset(&profiler.result[PROFILER].freq_stats, 0, sizeof(struct mif_freq_state) * NUM_IP);
		memset(&profiler.prev_wow_profile, 0, sizeof(struct exynos_wow_profile));
		memset(profiler.prev_tdata_in_state, 0, sizeof(u64) * profiler.table_cnt);

		return 0;
	}

	mifpro_update_profile(PROFILER);

	return 0;
}

u64 mifpro_get_freq_stats0_sum(void) { return profiler.result[PROFILER].freq_stats[MIF].sum; };
u64 mifpro_get_freq_stats0_avg(void) { return profiler.result[PROFILER].freq_stats[MIF].avg; };
u64 mifpro_get_freq_stats0_ratio(void) { return profiler.result[PROFILER].freq_stats[MIF].ratio; };
u64 mifpro_get_freq_stats1_sum(void) { return profiler.result[PROFILER].freq_stats[CPU].sum; };
u64 mifpro_get_freq_stats1_ratio(void) { return profiler.result[PROFILER].freq_stats[CPU].ratio; };
u64 mifpro_get_freq_stats2_sum(void) { return profiler.result[PROFILER].freq_stats[GPU].sum; };
u64 mifpro_get_freq_stats2_ratio(void) { return profiler.result[PROFILER].freq_stats[GPU].ratio; };
u64 mifpro_get_llc_status(void) { return profiler.result[PROFILER].llc_status; };

struct private_fn_mif mif_pd_fn = {
	.get_stats0_sum	= &mifpro_get_freq_stats0_sum,
	.get_stats0_ratio	= &mifpro_get_freq_stats0_ratio,
	.get_stats0_avg	= &mifpro_get_freq_stats0_avg,
	.get_stats1_sum	= &mifpro_get_freq_stats1_sum,
	.get_stats1_ratio	= &mifpro_get_freq_stats1_ratio,
	.get_stats2_sum	= &mifpro_get_freq_stats2_sum,
	.get_stats2_ratio	= &mifpro_get_freq_stats2_ratio,
	.get_llc_status = &mifpro_get_llc_status,
};

struct domain_fn mif_fn = {
	.get_table_cnt		= &mifpro_get_table_cnt,
	.get_freq_table		= &mifpro_get_freq_table,
	.get_max_freq		= &mifpro_get_max_freq,
	.get_min_freq		= &mifpro_get_min_freq,
	.get_freq		= &mifpro_get_freq,
	.get_power		= &mifpro_get_power,
	.get_power_change	= &mifpro_get_power_change,
	.get_active_pct		= &mifpro_get_active_pct,
	.get_temp		= &mifpro_get_temp,
	.set_margin		= &mifpro_set_margin,
	.update_mode		= &mifpro_update_mode,
};

/************************************************************************
 *			Gathering MIFFreq Information			*
 ************************************************************************/
//ktime_t * exynos_stats_get_mif_time_in_state(void);
extern u64 exynos_bcm_get_ccnt(unsigned int idx);
u32 mifpro_update_profile(int user)
{
	struct freq_cstate *fc = &profiler.fc;
	struct freq_cstate_snapshot *fc_snap = &profiler.fc_snap[user];
	struct freq_cstate_result *fc_result = &profiler.result[user].fc_result;
	struct mif_profile_result *result = &profiler.result[user];
	int i;
	u64 total_active_time = 0;
	static u64 *tdata_in_state = NULL;
	u64 transfer_data[NUM_IP] = { 0, };
	u64 mo_count[NUM_IP] = { 0, };
	u64 nr_requests[NUM_IP] = { 0, };
	u64 diff_ccnt = 0;
	struct exynos_wow_profile wow_profile = {0, };

	profiler.cur_freq_idx = get_idx_from_freq(profiler.table, profiler.table_cnt,
			*(profiler.freq_infos.cur_freq), RELATION_LOW);
	profiler.max_freq_idx = get_idx_from_freq(profiler.table, profiler.table_cnt,
			exynos_pm_qos_request(profiler.freq_infos.pm_qos_class_max), RELATION_LOW);
	profiler.min_freq_idx = get_idx_from_freq(profiler.table, profiler.table_cnt,
			exynos_pm_qos_request(profiler.freq_infos.pm_qos_class), RELATION_HIGH);

	if (!tdata_in_state)
		tdata_in_state = kzalloc(sizeof(u64) * profiler.table_cnt, GFP_KERNEL);

	exynos_devfreq_get_profile(profiler.devfreq_type, fc->time, tdata_in_state);
	exynos_wow_get_data(&wow_profile);

	// calculate delta from previous status
	make_snapshot_and_time_delta(fc, fc_snap, fc_result, profiler.table_cnt);

	// Call to calc power
	compute_freq_cstate_result(profiler.table, fc_result, profiler.table_cnt,
					profiler.cur_freq_idx, result->avg_temp);

	// Calculate freq_stats array
	for (i = 0; i < profiler.table_cnt; i++)
		result->tdata_in_state[i] = (tdata_in_state[i] - profiler.prev_tdata_in_state[i]) >> 20;

	diff_ccnt = (wow_profile.total_ccnt - profiler.prev_wow_profile.total_ccnt);
	if(wow_profile.transfer_data[MIF]) {
		for (i = 0 ; i < NUM_IP ; i++) {
			transfer_data[i] = (wow_profile.transfer_data[i] - profiler.prev_wow_profile.transfer_data[i]) >> 20;
			nr_requests[i] = wow_profile.nr_requests[i] - profiler.prev_wow_profile.nr_requests[i];
			mo_count[i] = wow_profile.mo_count[i] - profiler.prev_wow_profile.mo_count[i];
		}
	} else {
		for (i = 0 ; i < NUM_IP ; i++) {
			if (i == MIF) {
				continue;
			}
			transfer_data[MIF] += (wow_profile.transfer_data[i] - profiler.prev_wow_profile.transfer_data[i]) >> 20;
			nr_requests[MIF] += wow_profile.nr_requests[i] - profiler.prev_wow_profile.nr_requests[i];
			mo_count[MIF] += wow_profile.mo_count[i] - profiler.prev_wow_profile.mo_count[i];
		}
	}

	/* copy to prev buffer */
	memcpy(&profiler.prev_wow_profile, &wow_profile, sizeof(struct exynos_wow_profile));
	memcpy(profiler.prev_tdata_in_state, tdata_in_state, sizeof(u64) * profiler.table_cnt);

	for (i = 0 ; i < profiler.table_cnt; i++)
		fc->time[CLK_OFF][i] -= fc->time[CS_ACTIVE][i];

	memset(&result->freq_stats, 0, sizeof(struct mif_freq_state) * NUM_IP);
	fc_result->dyn_power = 0;
	for (i = 0; i < profiler.table_cnt; i++) {
		if (profiler.table[i].freq > 1000)
			result->freq_stats[MIF].avg += (result->tdata_in_state[i] << 40) / (profiler.table[i].freq / 1000);
		total_active_time += fc_result->time[CS_ACTIVE][i];
		if (fc_result->profile_time)
			fc_result->dyn_power += ((result->tdata_in_state[i] * profiler.table[i].dyn_cost) / fc_result->profile_time);
	}

	if (total_active_time)
		result->freq_stats[MIF].avg = result->freq_stats[MIF].avg / total_active_time;

	result->llc_status = llc_get_en();

	for (i = 0 ; i < NUM_IP ; i++) {
		if (fc_result->profile_time)
			result->freq_stats[i].sum = (transfer_data[i] * 1000000000) / fc_result->profile_time;
		if (diff_ccnt && nr_requests[i])
			result->freq_stats[i].ratio = ((fc_result->profile_time / diff_ccnt) * mo_count[i]) / nr_requests[i];
	}

	if (profiler.tz) {
		int temp = exynos_profiler_get_temp(profiler.tz);
		profiler.result[user].avg_temp = (temp + profiler.result[user].cur_temp) >> 1;
		profiler.result[user].cur_temp = temp;
	}

	return 0;
}

/************************************************************************
 *				INITIALIZATON				*
 ************************************************************************/
/* Initialize profiler data */
static int register_export_fn(u32 *max_freq, u32 *min_freq, u32 *cur_freq)
{
	struct exynos_devfreq_freq_infos *freq_infos = &profiler.freq_infos;

	exynos_devfreq_get_freq_infos(profiler.devfreq_type, freq_infos);

	*max_freq = freq_infos->max_freq;		/* get_org_max_freq(void) */
	*min_freq = freq_infos->min_freq;		/* get_org_min_freq(void) */
	*cur_freq = *freq_infos->cur_freq;		/* get_cur_freq(void)	  */
	profiler.table_cnt = freq_infos->max_state;	/* get_freq_table_cnt(void)  */

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

	ret = of_property_read_s32(dn, "devfreq-type", &profiler.devfreq_type);
	if (ret)
		profiler.devfreq_type = -1;	/* Don't support profiler */

	of_property_read_u32(dn, "power-coefficient", &profiler.dyn_pwr_coeff);
	of_property_read_u32(dn, "static-power-coefficient", &profiler.st_pwr_coeff);
//	of_property_read_string(dn, "tz-name", &profiler.tz_name);

	return 0;
}

static int init_profile_result(struct mif_profile_result *result, int size)
{
	if (init_freq_cstate_result(&result->fc_result, size))
		return -ENOMEM;

	/* init private data */
	result->tdata_in_state = alloc_state_in_freq(size);

	return 0;
}

#ifdef CONFIG_EXYNOS_DEBUG_INFO
static void show_profiler_info(void)
{
	int idx;

	pr_info("================ mif domain ================\n");
	pr_info("min= %dKHz, max= %dKHz\n",
			profiler.table[profiler.table_cnt - 1].freq, profiler.table[0].freq);
	for (idx = 0; idx < profiler.table_cnt; idx++)
		pr_info("lv=%3d freq=%8d volt=%8d dyn_cost=%lld st_cost=%lld\n",
			idx, profiler.table[idx].freq, profiler.table[idx].volt,
			profiler.table[idx].dyn_cost,
			profiler.table[idx].st_cost);
	if (profiler.tz_name)
		pr_info("support temperature (tz_name=%s)\n", profiler.tz_name);
	if (profiler.profiler_id != -1)
		pr_info("support profiler domain(id=%d)\n", profiler.profiler_id);
}
#endif

static int exynos_mif_profiler_probe(struct platform_device *pdev)
{
	unsigned int org_max_freq, org_min_freq, cur_freq;
	int ret, idx;

	/* get node of device tree */
	if (!pdev->dev.of_node) {
		pr_err("mifpro: failed to get device treee\n");
		return -EINVAL;
	}
	profiler.root = pdev->dev.of_node;

	/* Parse data from Device Tree to init domain */
	ret = parse_dt(profiler.root);
	if (ret) {
		pr_err("mifpro: failed to parse dt(ret: %d)\n", ret);
		return -EINVAL;
	}

	register_export_fn(&org_max_freq, &org_min_freq, &cur_freq);
	if (profiler.table_cnt < 1) {
		pr_err("mifpro: failed to get table_cnt\n");
		return -EINVAL;
	}

	/* init freq table */
	profiler.table = init_freq_table(NULL, profiler.table_cnt,
			profiler.cal_id, org_max_freq, org_min_freq,
			profiler.dyn_pwr_coeff, profiler.st_pwr_coeff,
			PWR_COST_CVV, PWR_COST_CVV);
	if (!profiler.table) {
		pr_err("mifpro: failed to init freq_table\n");
		return -EINVAL;
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
	profiler.prev_tdata_in_state = kzalloc(sizeof(u64) * profiler.table_cnt, GFP_KERNEL);

	/* get thermal-zone to get temperature */
	if (profiler.tz_name)
		profiler.tz = exynos_profiler_init_temp(profiler.tz_name);

	if (profiler.tz)
		init_static_cost(profiler.table, profiler.table_cnt,
				1, profiler.root, profiler.tz);

	ret = exynos_profiler_register_domain(PROFILER_MIF, &mif_fn, &mif_pd_fn);

#ifdef CONFIG_EXYNOS_DEBUG_INFO
	show_profiler_info();
#endif

	return ret;
}

static const struct of_device_id exynos_mif_profiler_match[] = {
	{
		.compatible	= "samsung,exynos-mif-profiler",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_mif_profiler_match);

static struct platform_driver exynos_mif_profiler_driver = {
	.probe		= exynos_mif_profiler_probe,
	.driver	= {
		.name	= "exynos-mif-profiler",
		.owner	= THIS_MODULE,
		.of_match_table = exynos_mif_profiler_match,
	},
};

static int exynos_mif_profiler_init(void)
{
	return platform_driver_register(&exynos_mif_profiler_driver);
}
late_initcall(exynos_mif_profiler_init);

MODULE_DESCRIPTION("Exynos MIF Profiler v1");
MODULE_SOFTDEP("pre: exynos_thermal_v2 exynos-cpufreq sgpu exynos_devfreq exynos-wow");
MODULE_LICENSE("GPL");
