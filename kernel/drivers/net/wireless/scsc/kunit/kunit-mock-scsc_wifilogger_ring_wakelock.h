/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SCSC_WIFILOGGER_RING_WAKELOCK_H__
#define __KUNIT_MOCK_SCSC_WIFILOGGER_RING_WAKELOCK_H__

#include "../scsc_wifilogger_ring_wakelock.h"

#define scsc_wifilogger_ring_wakelock_init(args...)	kunit_mock_scsc_wifilogger_ring_wakelock_init(args)
#define scsc_wifilogger_ring_wakelock_action(args...)	kunit_mock_scsc_wifilogger_ring_wakelock_action(args)


static bool kunit_mock_scsc_wifilogger_ring_wakelock_init(void)
{
	return false;
}

static int kunit_mock_scsc_wifilogger_ring_wakelock_action(u32 verbose_level, int status,
					 char *wl_name, int reason)
{
	return 0;
}
#endif
