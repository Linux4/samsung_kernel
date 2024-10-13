/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SCSC_WIFILOGGER_RING_TEST_H__
#define __KUNIT_MOCK_SCSC_WIFILOGGER_RING_TEST_H__

#include "../scsc_wifilogger_ring_test.h"

#define scsc_wifilogger_ring_test_init(args...)		kunit_mock_scsc_wifilogger_ring_test_init(args)
#define scsc_wifilogger_ring_test_write(args...)	kunit_mock_scsc_wifilogger_ring_test_write(args)


static bool kunit_mock_scsc_wifilogger_ring_test_init(void)
{
	return false;
}

static int kunit_mock_scsc_wifilogger_ring_test_write(char *buf, size_t blen)
{
	return 0;
}
#endif
