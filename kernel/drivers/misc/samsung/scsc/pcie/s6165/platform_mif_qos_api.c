/****************************************************************************
 *
 * Copyright (c) 2014 - 2023 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#if IS_ENABLED(CONFIG_SCSC_QOS)
#include "platform_mif_qos_api.h"
#if defined(CONFIG_SOC_S5E9945)
/* cpucl0: initial-level (lowest freq-level) */
static uint qos_cpucl0_lv[] = {0, 0, 0};
/* cpucl1: initial-level (lowest freq-level) */
static uint qos_cpucl1_lv[] = {0, 0, 0};
/* cpucl2: initial-level (lowest freq-level) */
static uint qos_cpucl2_lv[] = {0, 0, 0};
/* int: initial-level (lowest freq-level) */
static uint qos_int_lv[] = {7, 7, 7};
/* if: initial-level (lowest freq-level) */
static uint qos_mif_lv[] = {12, 12, 12};
#else
/* cpucl0: 400hz / 864Mhz / 1248Mhz for cpucl0 */
static uint qos_cpucl0_lv[] = {0, 4, 8};
/* cpucl1: 576hz / 1344Mhz / 2208Mhz for cpucl1 */
static uint qos_cpucl1_lv[] = {0, 8, 17};
/* cpucl2: 576hz / 1344Mhz / 2208Mhz for cpucl2 */
static uint qos_cpucl2_lv[] = {0, 8, 17};
/* int: 133hz / 332Mhz / 800Mhz */
static uint qos_int_lv[] = {7, 4, 0};
/* if: 421Mhz / 1716Mhz / 3172Mhz */
static uint qos_mif_lv[] = {12, 5, 0};
#endif /*CONFIG_SOC_S5E9945*/
module_param_array(qos_cpucl0_lv, uint, NULL, 0644);
MODULE_PARM_DESC(qos_cpucl0_lv, "S5E8535 DVFS Lv of CPUCL0 to apply Min/Med/Max PM QoS");

module_param_array(qos_cpucl1_lv, uint, NULL, 0644);
MODULE_PARM_DESC(qos_cpucl1_lv, "S5E8535 DVFS Lv of CPUCL1 to apply Min/Med/Max PM QoS");

module_param_array(qos_cpucl2_lv, uint, NULL, 0644);
MODULE_PARM_DESC(qos_cpucl2_lv, "S5E8535 DVFS Lv of CPUCL2 to apply Min/Med/Max PM QoS");

module_param_array(qos_int_lv, uint, NULL, 0644);
MODULE_PARM_DESC(qos_int_lv, "DVFS Level of INT");

module_param_array(qos_mif_lv, uint, NULL, 0644);
MODULE_PARM_DESC(qos_mif_lv, "DVFS Level of MIF");

static int platform_mif_set_affinity_cpu(struct scsc_mif_abs *interface, u8 msi, u8 cpu)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return pcie_mif_set_affinity_cpu(platform->pcie, msi, cpu);
}

int qos_get_param(char *buffer, const struct kernel_param *kp)
{
	/* To get cpu_policy of cl0 and cl1*/
	struct cpufreq_policy *cpucl0_policy = cpufreq_cpu_get(0);
	struct cpufreq_policy *cpucl1_policy = cpufreq_cpu_get(4);
	struct cpufreq_policy *cpucl2_policy = cpufreq_cpu_get(7);
	struct dvfs_rate_volt *int_domain_fv;
	struct dvfs_rate_volt *mif_domain_fv;
	u32 int_lv, mif_lv;
	int count = 0;

	count += sprintf(buffer + count, "CPUCL0 QoS: %u, %u, %u\n",
			cpucl0_policy->freq_table[qos_cpucl0_lv[0]].frequency,
			cpucl0_policy->freq_table[qos_cpucl0_lv[1]].frequency,
			cpucl0_policy->freq_table[qos_cpucl0_lv[2]].frequency);
	count += sprintf(buffer + count, "CPUCL1 QoS: %u, %u, %u\n",
			cpucl1_policy->freq_table[qos_cpucl1_lv[0]].frequency,
			cpucl1_policy->freq_table[qos_cpucl1_lv[1]].frequency,
			cpucl1_policy->freq_table[qos_cpucl1_lv[2]].frequency);

	count += sprintf(buffer + count, "CPUCL2 QoS: %u, %u, %u\n",
			cpucl2_policy->freq_table[qos_cpucl2_lv[0]].frequency,
			cpucl2_policy->freq_table[qos_cpucl2_lv[1]].frequency,
			cpucl2_policy->freq_table[qos_cpucl2_lv[2]].frequency);

	int_lv = cal_dfs_get_lv_num(ACPM_DVFS_INT);
	int_domain_fv =
		kcalloc(int_lv, sizeof(struct dvfs_rate_volt), GFP_KERNEL);
	cal_dfs_get_rate_asv_table(ACPM_DVFS_INT, int_domain_fv);
	count += sprintf(buffer + count, "INT QoS: %lu, %lu, %lu\n",
			int_domain_fv[qos_int_lv[0]].rate, int_domain_fv[qos_int_lv[1]].rate,
			int_domain_fv[qos_int_lv[2]].rate);

	mif_lv = cal_dfs_get_lv_num(ACPM_DVFS_MIF);
	mif_domain_fv =
		kcalloc(mif_lv, sizeof(struct dvfs_rate_volt), GFP_KERNEL);
	cal_dfs_get_rate_asv_table(ACPM_DVFS_MIF, mif_domain_fv);
	count += sprintf(buffer + count, "MIF QoS: %lu, %lu, %lu\n",
			mif_domain_fv[qos_mif_lv[0]].rate, mif_domain_fv[qos_mif_lv[1]].rate,
			mif_domain_fv[qos_mif_lv[2]].rate);

	cpufreq_cpu_put(cpucl0_policy);
	cpufreq_cpu_put(cpucl1_policy);
	cpufreq_cpu_put(cpucl2_policy);
	kfree(int_domain_fv);
	kfree(mif_domain_fv);

	return count;
}

const struct kernel_param_ops domain_id_ops = {
	.set = NULL,
	.get = &qos_get_param,
};
module_param_cb(qos_info, &domain_id_ops, NULL, 0444);

static void platform_mif_verify_qos_table(struct platform_mif *platform)
{
	u32 index;
	u32 cl0_max_idx, cl1_max_idx, cl2_max_idx;
	u32 int_max_idx, mif_max_idx;

	cl0_max_idx = cpufreq_frequency_table_get_index(platform->qos.cpucl0_policy,
							platform->qos.cpucl0_policy->max);
	cl1_max_idx = cpufreq_frequency_table_get_index(platform->qos.cpucl1_policy,
							platform->qos.cpucl1_policy->max);
	cl2_max_idx = cpufreq_frequency_table_get_index(platform->qos.cpucl2_policy,
							platform->qos.cpucl2_policy->max);

	if (cal_dfs_get_lv_num(ACPM_DVFS_INT))
		int_max_idx = (cal_dfs_get_lv_num(ACPM_DVFS_INT) - 1);
	else
		int_max_idx = 0;
	if (cal_dfs_get_lv_num(ACPM_DVFS_MIF))
		mif_max_idx = (cal_dfs_get_lv_num(ACPM_DVFS_MIF) - 1);
	else
		mif_max_idx = 0;

	for (index = SCSC_QOS_DISABLED; index < SCSC_QOS_MAX; index++) {
		qos_cpucl0_lv[index] = min(qos_cpucl0_lv[index], cl0_max_idx);
		qos_cpucl1_lv[index] = min(qos_cpucl1_lv[index], cl1_max_idx);
		qos_cpucl2_lv[index] = min(qos_cpucl2_lv[index], cl2_max_idx);
		qos_int_lv[index] = min(qos_int_lv[index], int_max_idx);
		qos_mif_lv[index] = min(qos_mif_lv[index], mif_max_idx);
	}
}

static int platform_mif_pm_qos_add_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (!platform)
		return -ENODEV;

	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	qos_req->cpu_cluster0_policy = cpufreq_cpu_get(0);
	qos_req->cpu_cluster1_policy = cpufreq_cpu_get(4);
	qos_req->cpu_cluster2_policy = cpufreq_cpu_get(7);

	if ((!qos_req->cpu_cluster0_policy) || (!qos_req->cpu_cluster1_policy) ||
			(!qos_req->cpu_cluster2_policy)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				"PM QoS add request error. CPU policy not loaded\n");
		return -ENOENT;
	}

	if (config == SCSC_QOS_DISABLED) {
		/* Update MIF/INT QoS */
		exynos_pm_qos_add_request(&qos_req->pm_qos_req_mif, PM_QOS_BUS_THROUGHPUT, 0);
		exynos_pm_qos_add_request(&qos_req->pm_qos_req_int, PM_QOS_DEVICE_THROUGHPUT, 0);

		/* Update Clusters QoS */
		freq_qos_tracer_add_request(&qos_req->cpu_cluster0_policy->constraints,
						&qos_req->pm_qos_req_cl0, FREQ_QOS_MIN, 0);
		freq_qos_tracer_add_request(&qos_req->cpu_cluster1_policy->constraints,
						&qos_req->pm_qos_req_cl1, FREQ_QOS_MIN, 0);
		freq_qos_tracer_add_request(&qos_req->cpu_cluster2_policy->constraints,
						&qos_req->pm_qos_req_cl2, FREQ_QOS_MIN, 0);

		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS add request: SCSC_QOS_DISABLED\n");
	} else {
		u32 qos_lv = config -1;
		/* Update MIF/INT QoS*/
		exynos_pm_qos_add_request(&qos_req->pm_qos_req_mif, PM_QOS_BUS_THROUGHPUT,
				platform->qos.mif_domain_fv[qos_mif_lv[qos_lv]].rate);
		exynos_pm_qos_add_request(&qos_req->pm_qos_req_int, PM_QOS_DEVICE_THROUGHPUT,
				platform->qos.int_domain_fv[qos_int_lv[qos_lv]].rate);

		/* Update Clusters QoS*/
		freq_qos_tracer_add_request(&qos_req->cpu_cluster0_policy->constraints,
										&qos_req->pm_qos_req_cl0, FREQ_QOS_MIN,
				platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[qos_lv]].frequency);
		freq_qos_tracer_add_request(&qos_req->cpu_cluster1_policy->constraints,
										&qos_req->pm_qos_req_cl1, FREQ_QOS_MIN,
				platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[qos_lv]].frequency);
		freq_qos_tracer_add_request(&qos_req->cpu_cluster2_policy->constraints,
										&qos_req->pm_qos_req_cl2, FREQ_QOS_MIN,
				platform->qos.cpucl2_policy->freq_table[qos_cpucl2_lv[qos_lv]].frequency);

		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			"PM QoS add request: %u. CL0 %u CL1 %u CL2 %u MIF %u INT %u\n", config,
			platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[qos_lv]].frequency,
			platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[qos_lv]].frequency,
			platform->qos.cpucl2_policy->freq_table[qos_cpucl2_lv[qos_lv]].frequency,
			platform->qos.mif_domain_fv[qos_mif_lv[qos_lv]].rate,
			platform->qos.int_domain_fv[qos_int_lv[qos_lv]].rate);
	}

	return 0;
}

static int platform_mif_pm_qos_update_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (!platform)
		return -ENODEV;

	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	if (config == SCSC_QOS_DISABLED) {
		/* Update MIF/INT QoS */
		exynos_pm_qos_update_request(&qos_req->pm_qos_req_mif, 0);
		exynos_pm_qos_update_request(&qos_req->pm_qos_req_int, 0);

		/* Update Clusters QoS */
		freq_qos_update_request(&qos_req->pm_qos_req_cl0, 0);
		freq_qos_update_request(&qos_req->pm_qos_req_cl1, 0);
		freq_qos_update_request(&qos_req->pm_qos_req_cl2, 0);

		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS update request: SCSC_QOS_DISABLED\n");
	} else {
		u32 qos_lv = config -1;
		/* Update MIF/INT QoS */
		exynos_pm_qos_update_request(&qos_req->pm_qos_req_mif,
				platform->qos.mif_domain_fv[qos_mif_lv[qos_lv]].rate);
		exynos_pm_qos_update_request(&qos_req->pm_qos_req_int,
				platform->qos.int_domain_fv[qos_int_lv[qos_lv]].rate);

		/* Update Clusters QoS */
		freq_qos_update_request(&qos_req->pm_qos_req_cl0,
			platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[qos_lv]].frequency);
		freq_qos_update_request(&qos_req->pm_qos_req_cl1,
			platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[qos_lv]].frequency);
		freq_qos_update_request(&qos_req->pm_qos_req_cl2,
			platform->qos.cpucl2_policy->freq_table[qos_cpucl2_lv[qos_lv]].frequency);

		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev,
			"PM QoS update request: %u. CL0 %u CL1 %u CL2 %u MIF %u INT %u \n", config,
			platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[qos_lv]].frequency,
			platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[qos_lv]].frequency,
			platform->qos.cpucl2_policy->freq_table[qos_cpucl2_lv[qos_lv]].frequency,
			platform->qos.mif_domain_fv[qos_mif_lv[qos_lv]].rate,
			platform->qos.int_domain_fv[qos_int_lv[qos_lv]].rate);
	}

	return 0;
}

static int platform_mif_pm_qos_remove_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (!platform)
		return -ENODEV;

	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
						"PM QoS remove request\n");

	/* Remove MIF/INT QoS */
	exynos_pm_qos_remove_request(&qos_req->pm_qos_req_mif);
	exynos_pm_qos_remove_request(&qos_req->pm_qos_req_int);

	/* Remove Clusters QoS */
	freq_qos_tracer_remove_request(&qos_req->pm_qos_req_cl0);
	freq_qos_tracer_remove_request(&qos_req->pm_qos_req_cl1);
	freq_qos_tracer_remove_request(&qos_req->pm_qos_req_cl2);

	return 0;
}

int platform_mif_qos_init(struct platform_mif *platform)
{
	struct scsc_mif_abs *interface = &platform->interface;
	u32 int_lv, mif_lv;
	interface->mif_pm_qos_add_request = platform_mif_pm_qos_add_request;
	interface->mif_pm_qos_update_request = platform_mif_pm_qos_update_request;
	interface->mif_pm_qos_remove_request = platform_mif_pm_qos_remove_request;
	interface->mif_set_affinity_cpu = platform_mif_set_affinity_cpu;

	int_lv = cal_dfs_get_lv_num(ACPM_DVFS_INT);
	mif_lv = cal_dfs_get_lv_num(ACPM_DVFS_MIF);

	platform->qos.cpucl0_policy = cpufreq_cpu_get(0);
	platform->qos.cpucl1_policy = cpufreq_cpu_get(4);
	platform->qos.cpucl2_policy = cpufreq_cpu_get(7);

	platform->qos_enabled = false;

	if ((!platform->qos.cpucl0_policy) || (!platform->qos.cpucl1_policy) ||
			(!platform->qos.cpucl2_policy)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				"PM QoS init error. CPU policy is not loaded\n");
		return -ENOENT;
	}

	platform->qos.int_domain_fv =
		devm_kzalloc(platform->dev, sizeof(struct dvfs_rate_volt) * int_lv, GFP_KERNEL);
	platform->qos.mif_domain_fv =
		devm_kzalloc(platform->dev, sizeof(struct dvfs_rate_volt) * mif_lv, GFP_KERNEL);

	cal_dfs_get_rate_asv_table(ACPM_DVFS_INT, platform->qos.int_domain_fv);
	cal_dfs_get_rate_asv_table(ACPM_DVFS_MIF, platform->qos.mif_domain_fv);

	if ((!platform->qos.int_domain_fv) || (!platform->qos.mif_domain_fv)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				"PM QoS init error. DEV(MIF,INT) policy is not loaded\n");
		return -ENOENT;
	}

	/* verify pre-configured freq-levels of cpucl0/1/2 */
	platform_mif_verify_qos_table(platform);

	platform->qos_enabled = true;

	return 0;
}
#else
void platfor_mif_qos_init(struct platform_mif *platform)
{
	(void)platform;
}
#endif
