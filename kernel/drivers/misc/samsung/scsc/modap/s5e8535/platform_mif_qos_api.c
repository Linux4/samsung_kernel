#include "platform_mif.h"

#if IS_ENABLED(CONFIG_SCSC_QOS)
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>
#include <soc/samsung/cal-if.h>
#include <dt-bindings/clock/s5e8535.h>

#include "scsc_mif_abs.h"

/* 533Mhz / 960Mhz / 1690Mhz for cpucl0 */
static uint qos_cpucl0_lv[] = {0, 0, 4, 12};
module_param_array(qos_cpucl0_lv, uint, NULL, 0644);
MODULE_PARM_DESC(qos_cpucl0_lv, "S5E8535 DVFS Lv of CPUCL0 to apply Min/Med/Max PM QoS");

/* 533Mhz / 960Mhz / 2132Mhz for cpucl1 */
static uint qos_cpucl1_lv[] = {0, 0, 4, 16};
module_param_array(qos_cpucl1_lv, uint, NULL, 0644);
MODULE_PARM_DESC(qos_cpucl1_lv, "S5E8535 DVFS Lv of CPUCL1 to apply Min/Med/Max PM QoS");

static int platform_mif_set_affinity_cpu(struct scsc_mif_abs *interface, u8 cpu)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Change CPU affinity to %d\n", cpu);
	return irq_set_affinity_hint(platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, cpumask_of(cpu));
}

int qos_get_param(char *buffer, const struct kernel_param *kp)
{
	/* To get cpu_policy of cl0 and cl1*/
	struct cpufreq_policy *cpucl0_policy = cpufreq_cpu_get(0);
	struct cpufreq_policy *cpucl1_policy = cpufreq_cpu_get(6);
	int count = 0;

	count += sprintf(buffer + count, "CPUCL0 QoS: %d, %d, %d\n",
			cpucl0_policy->freq_table[qos_cpucl0_lv[1]].frequency,
			cpucl0_policy->freq_table[qos_cpucl0_lv[2]].frequency,
			cpucl0_policy->freq_table[qos_cpucl0_lv[3]].frequency);
	count += sprintf(buffer + count, "CPUCL1 QoS: %d, %d, %d\n",
			cpucl1_policy->freq_table[qos_cpucl1_lv[1]].frequency,
			cpucl1_policy->freq_table[qos_cpucl1_lv[2]].frequency,
			cpucl1_policy->freq_table[qos_cpucl1_lv[3]].frequency);

	cpufreq_cpu_put(cpucl0_policy);
	cpufreq_cpu_put(cpucl1_policy);

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
	u32 cl0_max_idx, cl1_max_idx;

	cl0_max_idx = cpufreq_frequency_table_get_index(platform->qos.cpucl0_policy,
							platform->qos.cpucl0_policy->max);
	cl1_max_idx = cpufreq_frequency_table_get_index(platform->qos.cpucl1_policy,
							platform->qos.cpucl1_policy->max);

	for (index = SCSC_QOS_MIN; index <= SCSC_QOS_MAX; index++) {
		if (qos_cpucl0_lv[index] > cl0_max_idx)
			qos_cpucl0_lv[index] = cl0_max_idx;

		if (qos_cpucl1_lv[index] > cl1_max_idx)
			qos_cpucl1_lv[index] = cl1_max_idx;
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
	qos_req->cpu_cluster1_policy = cpufreq_cpu_get(6);

	if ((!qos_req->cpu_cluster0_policy) || (!qos_req->cpu_cluster1_policy)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				"PM QoS add request error. CPU policy not loaded\n");
		return -ENOENT;
	}

	if (config == SCSC_QOS_DISABLED) {
		freq_qos_tracer_add_request(
			&qos_req->cpu_cluster0_policy->constraints,
			&qos_req->pm_qos_req_cl0,
			FREQ_QOS_MIN,
			0);
		freq_qos_tracer_add_request(
			&qos_req->cpu_cluster1_policy->constraints,
			&qos_req->pm_qos_req_cl1,
			FREQ_QOS_MIN,
			0);
	} else {
		freq_qos_tracer_add_request(
			&qos_req->cpu_cluster0_policy->constraints,
			&qos_req->pm_qos_req_cl0,
			FREQ_QOS_MIN,
			platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[config]].frequency);
		freq_qos_tracer_add_request(
			&qos_req->cpu_cluster1_policy->constraints,
			&qos_req->pm_qos_req_cl1,
			FREQ_QOS_MIN,
			platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[config]].frequency);
	}

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"PM QoS add request: %u. CL0 %u CL1 %u\n", config,
		platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[config]].frequency,		platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[config]].frequency);

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
		freq_qos_update_request(&qos_req->pm_qos_req_cl0, 0);
		freq_qos_update_request(&qos_req->pm_qos_req_cl1, 0);
	} else {
		freq_qos_update_request(&qos_req->pm_qos_req_cl0,
			platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[config]].frequency);
		freq_qos_update_request(&qos_req->pm_qos_req_cl1,
			platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[config]].frequency);
	}
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev,
		"PM QoS add request: %u. CL0 %u CL1 %u\n", config,
		platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[config]].frequency,
		platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[config]].frequency);

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

	freq_qos_tracer_remove_request(&qos_req->pm_qos_req_cl0);
	freq_qos_tracer_remove_request(&qos_req->pm_qos_req_cl1);

	return 0;
}

int platform_mif_qos_init(struct platform_mif *platform)
{
	struct scsc_mif_abs *interface = &platform->interface;

	platform->qos.cpucl0_policy = cpufreq_cpu_get(0);
	platform->qos.cpucl1_policy = cpufreq_cpu_get(6);

	platform->qos_enabled = false;

	interface->mif_pm_qos_add_request = platform_mif_pm_qos_add_request;
	interface->mif_pm_qos_update_request = platform_mif_pm_qos_update_request;
	interface->mif_pm_qos_remove_request = platform_mif_pm_qos_remove_request;
	interface->mif_set_affinity_cpu = platform_mif_set_affinity_cpu;

	if ((!platform->qos.cpucl0_policy) || (!platform->qos.cpucl1_policy)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				"PM QoS init error. CPU policy is not loaded\n");
		return -ENOENT;
	}

	/* verify pre-configured freq-levels of cpucl0/1 */
	platform_mif_verify_qos_table(platform);

	platform->qos_enabled = true;

	return 0;
}
#else
void platform_mif_qos_init(struct platform_mif *platform)
{
	(void)platform;
}
#endif
