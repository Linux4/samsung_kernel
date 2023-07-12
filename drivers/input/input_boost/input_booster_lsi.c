#include <linux/input/input_booster.h>
#include <linux/cpufreq.h>
#include <linux/sched.h>
#include <linux/ems.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/exynos-ucc.h>

static struct emstune_mode_request emstune_req_input;
static struct pm_qos_request cluster2_qos;
static struct pm_qos_request cluster1_qos;
static struct pm_qos_request cluster0_qos;
static struct pm_qos_request mif_qos;
static struct pm_qos_request int_qos;
static struct ucc_req ucc_req = {
	.name = "input",
};

static DEFINE_MUTEX(input_lock);
bool current_hmp_boost = INIT_ZERO;
bool current_ucc_boost = INIT_ZERO;

struct inform {
  void *qos;
  void (*set_func)(int);
  int release_value;
};
struct inform informations[MAX_RES_COUNT];

void set_ib_ucc(int ucc_value)
{
	mutex_lock(&input_lock);

	if (ucc_value != current_ucc_boost) {
		pr_booster("[Input Booster2] ******      set_ucc : %d ( %s )\n", ucc_value, __FUNCTION__);
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
		pr_booster("[Input Booster2] ******      set_ehmp : %d ( %s )\n", hmp_value, __FUNCTION__);
		emstune_update_request(&emstune_req_input, hmp_value);
		current_hmp_boost = hmp_value;
	}

	mutex_unlock(&input_lock);
}

void ib_set_booster(long *qos_values)
{
	int res_type = 0;
	int cur_res_idx;
	long value = -1;

	for (res_type = 0; res_type < allowed_res_count; res_type++) {

		cur_res_idx = allowed_resources[res_type];
		value = qos_values[cur_res_idx];

		if (value <= 0) {
			pr_booster("[Input ooster2] ******      value : %d\n", value);
			continue;
		}

		if (informations[cur_res_idx].qos) {
			pr_booster("[Input ooster2] ******      qos value : %d\n", value);
			pm_qos_update_request(informations[cur_res_idx].qos, value);
		} else {
			pr_booster("[Input ooster2] ******      func value : %d\n", value);
			informations[cur_res_idx].set_func(value);
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

		if (informations[cur_res_idx].qos) {
			pm_qos_update_request(informations[cur_res_idx].qos, release_val[cur_res_idx]);
		} else {
			informations[cur_res_idx].set_func(release_val[cur_res_idx]);
		}
	}
}

int input_booster_init_vendor(void)
{
	int i = 0;
	pm_qos_add_request(&cluster2_qos, PM_QOS_CLUSTER2_FREQ_MIN, PM_QOS_CLUSTER2_FREQ_MIN_DEFAULT_VALUE);
	pm_qos_add_request(&cluster1_qos, PM_QOS_CLUSTER1_FREQ_MIN, PM_QOS_CLUSTER1_FREQ_MIN_DEFAULT_VALUE);
	pm_qos_add_request(&cluster0_qos, PM_QOS_CLUSTER0_FREQ_MIN, PM_QOS_CLUSTER0_FREQ_MIN_DEFAULT_VALUE);
	pm_qos_add_request(&mif_qos, PM_QOS_BUS_THROUGHPUT, PM_QOS_BUS_THROUGHPUT_DEFAULT_VALUE);
	pm_qos_add_request(&int_qos, PM_QOS_DEVICE_THROUGHPUT, PM_QOS_DEVICE_THROUGHPUT_DEFAULT_VALUE);

	informations[i++].qos = &cluster2_qos;
	informations[i++].qos = &cluster1_qos;
	informations[i++].qos = &cluster0_qos;
	informations[i++].qos = &mif_qos;
	informations[i++].qos = &int_qos;
	informations[i++].set_func = set_ib_hmp;
	informations[i++].set_func = set_ib_ucc;

	emstune_add_request(&emstune_req_input);

	return 1;
}

void input_booster_exit_vendor(void)
{
	pm_qos_remove_request(&cluster2_qos);
	pm_qos_remove_request(&cluster1_qos);
	pm_qos_remove_request(&cluster0_qos);
	pm_qos_remove_request(&mif_qos);
	pm_qos_remove_request(&int_qos);
}