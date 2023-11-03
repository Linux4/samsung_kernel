/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SLSI_TRACEPOINT_DEBUG_H__
#define __KUNIT_MOCK_SLSI_TRACEPOINT_DEBUG_H__

#include "../slsi_tracepoint_debug.h"

#define slsi_unregister_tracepoint_callback_all(args...)	kunit_mock_slsi_unregister_tracepoint_callback_all(args)
#define slsi_unregister_tracepoint_callback(args...)		kunit_mock_slsi_unregister_tracepoint_callback(args)
#define slsi_register_tracepoint_callback(args...)		kunit_mock_slsi_register_tracepoint_callback(args)
#define slsi_unregister_all_tracepoints(args...)		kunit_mock_slsi_unregister_all_tracepoints(args)


static int kunit_mock_slsi_unregister_tracepoint_callback_all(const char *tp_name)
{
	return 0;
}

static int kunit_mock_slsi_unregister_tracepoint_callback(const char *tp_name, void *func)
{
	return 0;
}

static int kunit_mock_slsi_register_tracepoint_callback(const char *tp_name, void *func, void *data)
{
	return 0;
}

static void kunit_mock_slsi_unregister_all_tracepoints(void)
{
	return;
}
#endif
