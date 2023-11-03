/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SLSI_CPUHP_MONITOR_H__
#define __KUNIT_MOCK_SLSI_CPUHP_MONITOR_H__

#include "../slsi_cpuhp_monitor.h"

#define slsi_cpuhp_monitor_init(args...)		kunit_mock_slsi_cpuhp_monitor_init(args)
#define slsi_cpuhp_monitor_deinit(args...)		kunit_mock_slsi_cpuhp_monitor_deinit(args)
#define slsi_cpuhp_monitor_register_callback(args...)	kunit_mock_slsi_cpuhp_monitor_register_callback(args)
#define slsi_cpuhp_monitor_unregister_callback(args...)	kunit_mock_slsi_cpuhp_monitor_unregister_callback(args)


static int kunit_mock_slsi_cpuhp_monitor_init(void)
{
	return 0;
}

static void kunit_mock_slsi_cpuhp_monitor_deinit(void)
{
	return;
}

static struct hlist_node *kunit_mock_slsi_cpuhp_monitor_register_callback(int (*online)(int cpu, void *data),
									  int (*offline)(int cpu, void *data),
									  void *data)
{
	return NULL;
}

static int kunit_mock_slsi_cpuhp_monitor_unregister_callback(struct hlist_node *node)
{
	return 0;
}
#endif
