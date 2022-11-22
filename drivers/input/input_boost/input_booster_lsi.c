#include <linux/input/input_booster.h>
#include <linux/cpufreq.h>
#include <linux/sched.h>
#include <linux/ems.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/exynos-ufcc.h>

static struct emstune_mode_request emstune_req_input;
struct freq_qos_request *cpu_cluster_qos;
struct exynos_pm_qos_request mif_qos;
struct exynos_pm_qos_request int_qos;
static bool cluster_qos_init_success;

static struct ucc_req ucc_req =
{
	.name = "input_booster",
};

static DEFINE_MUTEX(input_lock);
bool current_hmp_boost = INIT_ZERO;
bool current_ucc_boost = INIT_ZERO;

int freq_qos_init(void);

void set_ib_ucc(int ucc_value)
{
	mutex_lock(&input_lock);

	if (ucc_value != current_ucc_boost) {
		pr_booster("[Input Booster2] ******      set_ib_ucc : %d ( %s )\n", ucc_value, __FUNCTION__);
		if (ucc_value) {
			ucc_add_request(&ucc_req, ucc_value);
		} else {
			ucc_remove_request(&ucc_req);
		}
		current_ucc_boost = ucc_value;
	}

	mutex_unlock(&input_lock);
}

void set_ib_hmp(int hmp_value)
{
	mutex_lock(&input_lock);

	if (hmp_value != current_hmp_boost) {
		pr_booster("[Input Booster2] ******      set_ib_hmp : %d ( %s )\n", hmp_value, __FUNCTION__);
		emstune_update_request(&emstune_req_input, hmp_value);
		current_hmp_boost = hmp_value;
	}

	mutex_unlock(&input_lock);
}

void ib_set_booster(long* qos_values)
{
	int res_type = 0;
	int cur_res_idx;
	long value = -1;

	for (res_type = 0; res_type < allowed_res_count; res_type++) {

		cur_res_idx = allowed_resources[res_type];
		value = qos_values[cur_res_idx];

		if (value <= 0)
			continue;

		switch (cur_res_idx) {
		case CLUSTER2:
		case CLUSTER1:
		case CLUSTER0:
			mutex_lock(&input_lock);

			if (cluster_qos_init_success) {
				freq_qos_update_request(&cpu_cluster_qos[cur_res_idx],
					value);
			} else {
				freq_qos_init();
			}

			mutex_unlock(&input_lock);
			break;
		case MIF:
			exynos_pm_qos_update_request(&mif_qos, value);
			break;
		case INT:
			exynos_pm_qos_update_request(&int_qos, value);
			break;
		case HMPBOOST:
			set_ib_hmp(value);
			break;
		case UCC:
			set_ib_ucc(value);
			break;
		default:
			break;
		}
	}
}

void ib_release_booster(long *rel_flags)
{
	int res_type = 0;
	int cur_res_idx;
	long flag = -1;

	for (res_type = 0; res_type < allowed_res_count; res_type++) {

		cur_res_idx = allowed_resources[res_type];
		flag = rel_flags[cur_res_idx];

		if (flag <= 0)
			continue;

		switch (cur_res_idx) {
		case CLUSTER2:
		case CLUSTER1:
		case CLUSTER0:
			if (cpu_cluster_policy[cur_res_idx] != -1)
				freq_qos_update_request(&cpu_cluster_qos[cur_res_idx], release_val[cur_res_idx]);
			break;
		case MIF:
			exynos_pm_qos_update_request(&mif_qos, release_val[MIF]);
			break;
		case INT:
			exynos_pm_qos_update_request(&int_qos, release_val[INT]);
			break;
		case HMPBOOST:
			set_ib_hmp(release_val[HMPBOOST]);
			break;
		case UCC:
			set_ib_ucc(release_val[UCC]);
			break;
		default:
			break;
		}
	}
}

int freq_qos_init(void)
{
	int cpu_cluster;
	int ret;
	int fail_cluster;

	struct cpufreq_policy *policy;
	struct freq_qos_request *req;

	for (cpu_cluster = 0; cpu_cluster < max_cluster_count; cpu_cluster++) {
		if (cpu_cluster_policy[cpu_cluster] == -1)
			continue;

		policy = cpufreq_cpu_get(cpu_cluster_policy[cpu_cluster]);
		if (!policy) {
			pr_err("%s: Failed to get cpufreq policy for cluster(%d)\n",
				__func__, cpu_cluster_policy[cpu_cluster]);
			fail_cluster = cpu_cluster;
			ret = -EAGAIN;
			goto reset_qos;
		}
		ret = freq_qos_add_request(&policy->constraints,
			&cpu_cluster_qos[cpu_cluster], FREQ_QOS_MIN, policy->min);

		if (ret < 0) {
			pr_err("%s: Failed to add qos constraint (%d)\n",
				__func__, ret);
			fail_cluster = cpu_cluster;
			goto reset_qos;
		}
	}
	cluster_qos_init_success = true;
	return 0;

reset_qos:
	for (cpu_cluster = fail_cluster - 1; cpu_cluster >= 0; cpu_cluster--) {
		if (cpu_cluster_policy[cpu_cluster] == -1)
			freq_qos_remove_request(&cpu_cluster_qos[cpu_cluster]);
	}
	return ret;
}

int input_booster_init_vendor(void)
{

	int res_type = 0;

	cpu_cluster_qos = kcalloc(ABS_CNT, sizeof(struct freq_qos_request) * max_cluster_count, GFP_KERNEL);
	if (cpu_cluster_qos == NULL)
		return 0;

	for (res_type = 0; res_type < allowed_res_count; res_type++) {
		switch (allowed_resources[res_type]) {
		case MIF:
			exynos_pm_qos_add_request(&mif_qos,
				PM_QOS_BUS_THROUGHPUT, PM_QOS_BUS_THROUGHPUT_DEFAULT_VALUE);
			break;
		case INT:
			exynos_pm_qos_add_request(&int_qos,
				PM_QOS_DEVICE_THROUGHPUT, PM_QOS_DEVICE_THROUGHPUT_DEFAULT_VALUE);
			break;
		case HMPBOOST:
			emstune_add_request(&emstune_req_input);
			break;
		case UCC:
			ucc_add_request(&ucc_req, DEFAULT_LEVEL);
			break;
		default:
			break;
		}
	}

	return 1;
}

void input_booster_exit_vendor()
{
	int res_type = 0;

	for (res_type = 0; res_type < allowed_res_count; res_type++) {
		switch (allowed_resources[res_type]) {
		case CLUSTER2:
			if (cpu_cluster_policy[CLUSTER2] != -1)
				freq_qos_remove_request(&cpu_cluster_qos[CLUSTER2]);
			break;
		case CLUSTER1:
			if (cpu_cluster_policy[CLUSTER1] != -1)
				freq_qos_remove_request(&cpu_cluster_qos[CLUSTER1]);
			break;
		case CLUSTER0:
			if (cpu_cluster_policy[CLUSTER0] != -1)
				freq_qos_remove_request(&cpu_cluster_qos[CLUSTER0]);
			break;
		case MIF:
			exynos_pm_qos_remove_request(&mif_qos);
			break;
		case INT:
			exynos_pm_qos_remove_request(&int_qos);
			break;
		case HMPBOOST:
			break;
		case UCC:
			break;
		default:
			break;
		}
	}
	kfree(cpu_cluster_qos);
}
