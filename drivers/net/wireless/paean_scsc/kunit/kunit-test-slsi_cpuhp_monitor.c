// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-kernel.h"
#include "../slsi_cpuhp_monitor.c"

int test_online(int cpu, void *data)
{
	return 0;
}

int test_offline(int cpu, void *data)
{
	return 0;
}

static void test_slsi_cpuhp_online(struct kunit *test)
{
	struct cpuhp_cb_node *cb_node;
	struct hlist_node *node;

	cb_node = kunit_kzalloc(test, sizeof(struct cpuhp_cb_node), GFP_KERNEL);
	node = &cb_node->node;

	cb_node->online = test_online;

	KUNIT_EXPECT_EQ(test, 0, slsi_cpuhp_online(0U, node));
}

static void test_slsi_cpuhp_offline(struct kunit *test)
{
	struct cpuhp_cb_node *cb_node;
	struct hlist_node *node;

	cb_node = kunit_kzalloc(test, sizeof(struct cpuhp_cb_node), GFP_KERNEL);
	node = &cb_node->node;

	cb_node->offline = test_offline;

	KUNIT_EXPECT_EQ(test, 0, slsi_cpuhp_offline(0, node));
}

static void test_slsi_cpuhp_monitor_register_callback(struct kunit *test)
{
	int test_data = 0;
	struct cpuhp_cb_node *new_node;
	struct hlist_node *node;

	node = slsi_cpuhp_monitor_register_callback(test_online, test_offline, &test_data);

	slsi_cpuhp_monitor_init();
	node = slsi_cpuhp_monitor_register_callback(test_online, test_offline, &test_data);
	if (node) {
		new_node = container_of(node, struct cpuhp_cb_node, node);
		kfree(new_node);
	}
	slsi_cpuhp_monitor_deinit();
}

static void test_slsi_cpuhp_monitor_unregister_callback(struct kunit *test)
{
	struct cpuhp_cb_node *cb_node;
	struct hlist_node *node;

	KUNIT_EXPECT_EQ(test, -ENODEV, slsi_cpuhp_monitor_unregister_callback((void *)0));

	slsi_cpuhp_monitor_init();
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_cpuhp_monitor_unregister_callback((void *)0));

	cb_node = kzalloc(sizeof(struct cpuhp_cb_node), GFP_KERNEL);
	node = &cb_node->node;
	KUNIT_EXPECT_EQ(test, 0, slsi_cpuhp_monitor_unregister_callback(node));
	slsi_cpuhp_monitor_deinit();
}

static int slsi_cpuhp_monitor_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void slsi_cpuhp_monitor_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

static struct kunit_case slsi_cpuhp_monitor_test_cases[] = {
	KUNIT_CASE(test_slsi_cpuhp_online),
	KUNIT_CASE(test_slsi_cpuhp_offline),
	KUNIT_CASE(test_slsi_cpuhp_monitor_register_callback),
	KUNIT_CASE(test_slsi_cpuhp_monitor_unregister_callback),
	{}
};

static struct kunit_suite slsi_cpuhp_monitor_test_suite[] = {
	{
		.name = "kunit-slsi_cpuhp_monitor-test",
		.test_cases = slsi_cpuhp_monitor_test_cases,
		.init = slsi_cpuhp_monitor_test_init,
		.exit = slsi_cpuhp_monitor_test_exit,
	}
};

kunit_test_suites(slsi_cpuhp_monitor_test_suite);
