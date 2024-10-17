/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_TRAFFIC_MONITOR_H__
#define __KUNIT_MOCK_TRAFFIC_MONITOR_H__

#include "../traffic_monitor.h"

#define slsi_traffic_mon_client_unregister(args...)	kunit_mock_slsi_traffic_mon_client_unregister(args)
#define slsi_traffic_mon_clients_deinit(args...)	kunit_mock_slsi_traffic_mon_clients_deinit(args)
#define slsi_traffic_mon_clients_init(args...)		kunit_mock_slsi_traffic_mon_clients_init(args)
#define slsi_traffic_mon_override(args...)		kunit_mock_slsi_traffic_mon_override(args)
#define slsi_traffic_mon_client_register(args...)	kunit_mock_slsi_traffic_mon_client_register(args)
#define slsi_traffic_mon_is_running(args...)		kunit_mock_slsi_traffic_mon_is_running(args)
#define slsi_traffic_mon_init(args...)			kunit_mock_slsi_traffic_mon_init(args)
#define slsi_traffic_mon_deinit(args...)		kunit_mock_slsi_traffic_mon_deinit(args)


static void kunit_mock_slsi_traffic_mon_client_unregister(struct slsi_dev *sdev, void *client_ctx)
{
	return;
}

static void kunit_mock_slsi_traffic_mon_clients_deinit(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_slsi_traffic_mon_clients_init(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_slsi_traffic_mon_override(struct slsi_dev *sdev)
{
	return;
}

static int kunit_mock_slsi_traffic_mon_client_register(
	struct slsi_dev *sdev,
	void *client_ctx,
	u32 mode,
	u32 mid_tput,
	u32 high_tput,
	u32 dir,
	void (*traffic_mon_client_cb)(void *client_ctx, u32 state, u32 tput_tx, u32 tput_rx))
{
	if (!(sdev && client_ctx)) return 1;
	else return 0;
}

static u8 kunit_mock_slsi_traffic_mon_is_running(struct slsi_dev *sdev)
{
	return 0;
}

static void kunit_mock_slsi_traffic_mon_init(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_slsi_traffic_mon_deinit(struct slsi_dev *sdev)
{
	return;
}
#endif
