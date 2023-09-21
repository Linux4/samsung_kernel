/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_LOG_CLIENTS_H__
#define __KUNIT_MOCK_LOG_CLIENTS_H__

#include "../log_clients.h"

#define slsi_log_client_register(args...)		kunit_mock_slsi_log_client_register(args)
#define slsi_log_clients_log_signal_safe(args...)	kunit_mock_slsi_log_clients_log_signal_safe(args)
#define slsi_log_client_msg(args...)			kunit_mock_slsi_log_client_msg(args)
#define slsi_log_client_unregister(args...)		kunit_mock_slsi_log_client_unregister(args)
#define slsi_log_clients_init(args...)			kunit_mock_slsi_log_clients_init(args)
#define slsi_log_clients_terminate(args...)		kunit_mock_slsi_log_clients_terminate(args)
#define slsi_log_clients_log_signal_fast(args...)	kunit_mock_slsi_log_clients_log_signal_fast(args)


static int kunit_mock_slsi_log_client_register(struct slsi_dev *sdev, void *log_client_ctx,
					       int (*log_client_cb)(struct slsi_log_client *, struct sk_buff *, int),
					       char *filter, int min_signal_id, int max_signal_id)
{
	if (filter)
		kfree(filter);

	return 0;
}

static void kunit_mock_slsi_log_clients_log_signal_safe(struct slsi_dev *sdev, struct sk_buff *skb, u32 direction)
{
	return;
}

static void kunit_mock_slsi_log_client_msg(struct slsi_dev *sdev, u16 event, u32 event_data_length,
					   const u8 *event_data)
{
	return;
}

static void kunit_mock_slsi_log_client_unregister(struct slsi_dev *sdev, void *log_client_ctx)
{
	return;
}

static void kunit_mock_slsi_log_clients_init(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_slsi_log_clients_terminate(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_slsi_log_clients_log_signal_fast(struct slsi_dev *sdev, struct slsi_log_clients *log_clients,
							struct sk_buff *skb, u32 direction)
{
	return;
}
#endif
