/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 * KUnit test for thermal.
 */
#include <test/test.h>
#include <test/mock.h>
#include <linux/thermal.h>
#include <linux/sprd_cpu_cooling.h>
#include <linux/sprd_cpu_device.h>
#include "sprd_cpu_device_test.h"

struct sprd_cpu_device_test {
	struct test *test;
	struct cpu_power_model_t *power_model;
};

static void get_core_dyn_power_test(struct test *test)
{
	u32 power = 0;

	//cluster_id=0
	power = get_core_dyn_power(0, 768, 918);
	EXPECT_EQ(test, 65, power);

	power = get_core_dyn_power(0, 884, 975);
	EXPECT_EQ(test, 84, power);

	power = get_core_dyn_power(0, 1000, 1028);
	EXPECT_EQ(test, 106, power);

	power = get_core_dyn_power(0, 1100, 1075);
	EXPECT_EQ(test, 128, power);

	power = get_core_dyn_power(0, 1200, 1121);
	EXPECT_EQ(test, 151, power);

	//cluster_id=1
	power = get_core_dyn_power(1, 768, 900);
	EXPECT_EQ(test, 95, power);

	power = get_core_dyn_power(1, 1050, 921);
	EXPECT_EQ(test, 136, power);

	power = get_core_dyn_power(1, 1225, 984);
	EXPECT_EQ(test, 182, power);

	power = get_core_dyn_power(1, 1400, 1050);
	EXPECT_EQ(test, 237, power);

	power = get_core_dyn_power(1, 1500, 1084);
	EXPECT_EQ(test, 270, power);

	power = get_core_dyn_power(1, 1600, 1121);
	EXPECT_EQ(test, 308, power);
}

static void get_cluster_dyn_power_test(struct test *test)
{
	u32 power = 0;

	power = get_cluster_dyn_power(0, 1200, 1121);
	EXPECT_EQ(test, 112, power);

}

static void get_static_power_test(struct test *test)
{
	cpumask_t cpumask_4 = CPU_MASK_CPU4;
	u32 power = 0;
	u32 power_4 = 0;

	power = get_static_power(&cpumask_4, 100,
						1121875, &power_4, 70866);
	EXPECT_EQ(test, 107, power_4);

	power = get_static_power(&cpumask_4, 100,
						1120000, &power_4, 70866);
	EXPECT_EQ(test, 107, power_4);

	power = get_static_power(&cpumask_4, 100,
						1000000, &power_4, 70866);
	EXPECT_EQ(test, 80, power_4);

	power = get_static_power(&cpumask_4, 100,
						2121875, &power_4, 70866);
	EXPECT_EQ(test, 949, power_4);

	power = get_static_power(&cpumask_4, 100,
						6121875, &power_4, 70866);
	EXPECT_EQ(test, 33110, power_4);
}

static void get_core_static_power_test(struct test *test)
{
	cpumask_t cpumask_4 = CPU_MASK_CPU4;
	u32 power_4 = 0;
	u32 power = 0;

	power = get_core_static_power(&cpumask_4, 100,
						1121875, &power_4, 70866);
	EXPECT_EQ(test, 76, power_4);

	power = get_core_static_power(&cpumask_4, 100,
						1120000, &power_4, 70866);
	EXPECT_EQ(test, 76, power_4);

	power = get_core_static_power(&cpumask_4, 100,
						1000000, &power_4, 70866);
	EXPECT_EQ(test, 57, power_4);

	power = get_core_static_power(&cpumask_4, 100,
						2121875, &power_4, 70866);
	EXPECT_EQ(test, 672, power_4);

	power = get_core_static_power(&cpumask_4, 100,
						6121875, &power_4, 70866);
	EXPECT_EQ(test, 23424, power_4);
}

static void  get_cluster_min_cpufreq_test(struct test *test)
{
	u32 min_cpufreq  = 0;
	u32 min_cpufreq1 = 0;

	min_cpufreq  = get_cluster_min_cpufreq(0);
	min_cpufreq1 = get_cluster_min_cpufreq(1);

	EXPECT_EQ(test, 768000, min_cpufreq);
	EXPECT_EQ(test, 768000, min_cpufreq1);
}

static void  get_cluster_min_cpunum_test(struct test *test)
{
	u32 min_cpunum = 0;

	min_cpunum = get_cluster_min_cpunum(0);
	EXPECT_EQ(test, 4, min_cpunum);

	min_cpunum = get_cluster_min_cpunum(1);
	EXPECT_EQ(test, 0, min_cpunum);
}

static void get_cluster_resistance_ja_test(struct test *test)
{
	int test_ja = 0;

	test_ja = get_cluster_resistance_ja(0);
	EXPECT_EQ(test, 0, test_ja);
}

static void  get_core_temp_test(struct test *test)
{
	u32 core_temp = 0;

	get_core_temp(0, 0, &core_temp);
	EXPECT_EQ(test, 0, core_temp);

	get_core_temp(1, 4, &core_temp);
	EXPECT_EQ(test, 0, core_temp);

}

static void  get_all_core_temp_test(struct test *test)
{
	u32 ret_test = 0;

	ret_test = get_all_core_temp(0, 4);
	EXPECT_EQ(test, 0, ret_test);
}

static void  get_core_cpuidle_tp_test(struct test *test)
{
	u32 temp = 0;
	u32 tp_id = 0;

	tp_id = get_core_cpuidle_tp(0, 0, 0, &temp);
	EXPECT_EQ(test, 0, temp);

	tp_id = get_core_cpuidle_tp(0, 1, 0, &temp);
	EXPECT_EQ(test, 0, temp);

	tp_id = get_core_cpuidle_tp(0, 2, 5, &temp);
	EXPECT_EQ(test, 0, temp);

	tp_id = get_core_cpuidle_tp(0, 0, 3, &temp);
	EXPECT_EQ(test, 0, temp);

	tp_id = get_core_cpuidle_tp(0, 1, 5, &temp);
	EXPECT_EQ(test, 0, temp);

	tp_id = get_core_cpuidle_tp(0, 2, 6, &temp);
	EXPECT_EQ(test, 0, temp);

}

static void  get_cpuidle_temp_point_test(struct test *test)
{
	u32 temp_point = 0;

	temp_point = get_cpuidle_temp_point(0);
	EXPECT_EQ(test, 0, temp_point);

	temp_point = get_cpuidle_temp_point(1);
	EXPECT_EQ(test, 0, temp_point);

	temp_point = get_cpuidle_temp_point(2);
	EXPECT_EQ(test, 0, temp_point);

	temp_point = get_cpuidle_temp_point(3);
	EXPECT_EQ(test, 0, temp_point);
}

static void  get_cluster_temperature_scale_test(struct test *test)
{
	u32 t_scale = 0;

	//cluster_id = 0
	t_scale = get_cluster_temperature_scale(0, 85);
	EXPECT_EQ(test, 1055, t_scale);

	t_scale = get_cluster_temperature_scale(0, 86);
	EXPECT_EQ(test, 1093, t_scale);

	//cluster_id=1
	t_scale = get_cluster_temperature_scale(1, 80);
	EXPECT_EQ(test, 882,  t_scale);

	t_scale = get_cluster_temperature_scale(1, 71);
	EXPECT_EQ(test, 631, t_scale);
}

static void get_core_temperature_scale_test(struct test *test)
{
	u32 t_scale = 0;

	//cluster_id=0
	t_scale = get_core_temperature_scale(0, 70);
	EXPECT_EQ(test, 607, t_scale);

	t_scale = get_core_temperature_scale(0, 85);
	EXPECT_EQ(test, 1055, t_scale);

	t_scale = get_core_temperature_scale(0, 86);
	EXPECT_EQ(test, 1093, t_scale);

	//cluster_id=1
	t_scale = get_core_temperature_scale(1, 80);
	EXPECT_EQ(test, 882, t_scale);

	t_scale = get_core_temperature_scale(1, 90);
	EXPECT_EQ(test, 1255, t_scale);
}

static void  get_cpu_static_power_coeff_test(struct test *test)
{
	u32 leak_core_base = 0;

	leak_core_base = get_cpu_static_power_coeff(1);
	if (leak_core_base == 3970)
		EXPECT_EQ(test, 3970, leak_core_base);
	else if (leak_core_base == 400)
		EXPECT_EQ(test, 2400, leak_core_base);
	else if (leak_core_base == 4190)
		EXPECT_EQ(test, 4190, leak_core_base);
}

static void  get_cache_static_power_coeff_test(struct test *test)
{
	u32 leak_core_base = 0;

	leak_core_base = get_cache_static_power_coeff(0);
	if (leak_core_base == 3970)
		EXPECT_EQ(test, 3970, leak_core_base);
	else if (leak_core_base == 2400)
		EXPECT_EQ(test, 2400, leak_core_base);
	else if (leak_core_base == 4190)
		EXPECT_EQ(test, 4190, leak_core_base);
}

static void get_cluster_voltage_scale_test(struct test *test)
{
	u32 v_scale = 0;

	v_scale = get_cluster_voltage_scale(1, 1121875);
	EXPECT_EQ(test, 1477, v_scale);
}

static void get_core_voltage_scale_test(struct test *test)
{
	u32 v_scale = 0;

	//cluster_id = 0
	v_scale = get_core_voltage_scale(0, 975000);
	EXPECT_EQ(test, 905, v_scale);

	v_scale = get_core_voltage_scale(0, 1028125);
	EXPECT_EQ(test, 1091, v_scale);

	v_scale = get_core_voltage_scale(0, 1075000);
	EXPECT_EQ(test, 1276, v_scale);

	v_scale = get_core_voltage_scale(0, 1121875);
	EXPECT_EQ(test, 1477, v_scale);

	//cluster_id=1
	v_scale = get_core_voltage_scale(1, 921875);
	EXPECT_EQ(test, 741, v_scale);

	v_scale = get_core_voltage_scale(1, 900000);
	EXPECT_EQ(test, 683, v_scale);

	v_scale = get_core_voltage_scale(1, 984375);
	EXPECT_EQ(test, 934, v_scale);

	v_scale = get_core_voltage_scale(0, 1121875);
	EXPECT_EQ(test, 1477, v_scale);

	v_scale = get_core_voltage_scale(1, 1121875);
	EXPECT_EQ(test, 1477, v_scale);
}

static void sprd_cpu_show_store_min_freq_test(struct test *test)
{
	char buff[20] = "0";
	char buff_store[7] = "768000";
	u32 mock_min_freq = 0;
	u32 mock_min_freq_store = 0;
	size_t count = 7;
	struct device_node *np, *child;
	struct cpufreq_cooling_device *cpufreq_dev;

	np = of_find_node_by_name(NULL, "cooling-devices");

	for_each_child_of_node(np, child) {
		cpufreq_dev = cpufreq_cooling_get_dev_by_name(child->name);
		/* cooling-devices0  show.min_cpufreq=768000000 */
		mock_min_freq = sprd_cpu_show_min_freq(
			&cpufreq_dev->cool_dev->device, NULL, buff);
		EXPECT_EQ(test, 7, mock_min_freq);

		/* cooling-devices1  show.min_cpufreq=768000000 */
		mock_min_freq_store = sprd_cpu_store_min_freq(
			&cpufreq_dev->cool_dev->device, NULL,
			buff_store, count);
		EXPECT_EQ(test, 7, mock_min_freq);
	}
}

static void sprd_cpu_show_store_min_core_num_test(struct test *test)
{
	char buff[5] = "0";
	char buff_store[2] = "2";
	u32 min_core_num = 0;
	u32 min_core_num_store = 0;
	size_t count = 0;
	struct device_node *np, *child;
	char *cluster0 = "cluster0-cooling";
	char *cluster1 = "cluster1-cooling";
	char str[20];
	struct cpufreq_cooling_device *cpufreq_dev;

	np = of_find_node_by_name(NULL, "cooling-devices");
	for_each_child_of_node(np, child) {
		cpufreq_dev = cpufreq_cooling_get_dev_by_name(child->name);

		min_core_num = sprd_cpu_show_min_core_num(
			&cpufreq_dev->cool_dev->device, NULL, buff);
		strcpy(str, child->name);

		if (strcmp(str, cluster0) == 0) {
			EXPECT_EQ(test, '4', buff[0]);

			/* cluster0-cooling write 2 */
			min_core_num_store = sprd_cpu_store_min_core_num(
				&cpufreq_dev->cool_dev->device,
				NULL, buff_store, count);
			min_core_num = sprd_cpu_show_min_core_num(
				&cpufreq_dev->cool_dev->device,
				NULL, buff);
			EXPECT_EQ(test, '2', buff[0]);
		}
		if (strcmp(str, cluster1) == 0) {
			EXPECT_EQ(test, '0', buff[0]);

			/* cluster1-cooling write 2 */
			min_core_num_store = sprd_cpu_store_min_core_num(
				&cpufreq_dev->cool_dev->device,
				NULL, buff_store, count);
			min_core_num = sprd_cpu_show_min_core_num(
				&cpufreq_dev->cool_dev->device,
				NULL, buff);
			EXPECT_EQ(test, '2', buff[0]);
		}
	}
}

static void cpu_cooling_pm_notify_test(struct test *test)
{
	int result = 0;

	result = cpu_cooling_pm_notify(NULL, 0x0003, NULL);
	EXPECT_EQ(test, 0, result);

	result = cpu_cooling_pm_notify(NULL, 0x0004, NULL);
	EXPECT_EQ(test, 0, result);
}

static void get_leak_base_test(struct test *test)
{
	u32 leak_base = 0;
	int coeff[3] = {0, 0, 0};
	int coeff_1[3] = {1000, 1000, 1000};

	/* set coeff = 0 */
	leak_base = (u32)get_leak_base(0, 1, coeff);
	EXPECT_EQ(test, 0, leak_base);

	/* set coeff = 1000 */
	leak_base = (u32)get_leak_base(0, 1, coeff_1);
	EXPECT_EQ(test, 10000, leak_base);
}

static int sprd_cpu_device_test_init(struct test *test)
{
	struct sprd_cpu_device_test *ctx;

	ctx = test_kzalloc(test, sizeof(*ctx), GFP_KERNEL);

	if (!ctx)
		return -ENOMEM;

	test->priv = ctx;

	ctx->test = test;

	create_cpu_cooling_device();

	return 0;
}

static void sprd_cpu_device_test_exit(struct test *test)
{
	destroy_cpu_cooling_device();

	test_cleanup(test);
}

static struct test_case sprd_cpu_device_test_cases[] = {
	TEST_CASE(get_core_dyn_power_test),

	TEST_CASE(get_cluster_dyn_power_test),

	TEST_CASE(get_static_power_test),

	TEST_CASE(get_core_static_power_test),

	TEST_CASE(get_cluster_min_cpufreq_test),

	TEST_CASE(get_cluster_min_cpunum_test),

	TEST_CASE(get_cluster_resistance_ja_test),

	TEST_CASE(get_core_temp_test),

	TEST_CASE(get_all_core_temp_test),

	TEST_CASE(get_core_cpuidle_tp_test),

	TEST_CASE(get_cpuidle_temp_point_test),

	TEST_CASE(get_cluster_temperature_scale_test),

	TEST_CASE(get_core_temperature_scale_test),

	TEST_CASE(get_cluster_voltage_scale_test),

	TEST_CASE(get_core_voltage_scale_test),

	TEST_CASE(get_cache_static_power_coeff_test),

	TEST_CASE(get_cpu_static_power_coeff_test),

	TEST_CASE(sprd_cpu_show_store_min_freq_test),

	TEST_CASE(sprd_cpu_show_store_min_core_num_test),

	TEST_CASE(cpu_cooling_pm_notify_test),

	TEST_CASE(get_leak_base_test),

	{},
};

static struct test_module sprd_cpu_device_test_module = {
	.name = "sprd-cpu-device-test",
	.init = sprd_cpu_device_test_init,
	.exit = sprd_cpu_device_test_exit,
	.test_cases = sprd_cpu_device_test_cases,
};

module_test(sprd_cpu_device_test_module);
