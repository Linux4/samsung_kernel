/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SCSC_WIFILOGGER_RING_CONNECTIVITY_H__
#define __KUNIT_MOCK_SCSC_WIFILOGGER_RING_CONNECTIVITY_H__

#include "../scsc_wifilogger_ring_connectivity.h"

#define scsc_wifilogger_ring_connectivity_driver_event(args...)	kunit_mock_scsc_wifilogger_ring_connectivity_driver_event(args)
#define scsc_wifilogger_ring_connectivity_init(args...)		kunit_mock_scsc_wifilogger_ring_connectivity_init(args)
#define scsc_wifilogger_ring_connectivity_fw_event(args...)	kunit_mock_scsc_wifilogger_ring_connectivity_fw_event(args)


static int kunit_mock_scsc_wifilogger_ring_connectivity_driver_event(wlog_verbose_level lev,
								     u16 driver_event_id,
								     unsigned int tag_count, ...)
{
	return 0;
}

static bool kunit_mock_scsc_wifilogger_ring_connectivity_init(void)
{
	return 0;
}

static int kunit_mock_scsc_wifilogger_ring_connectivity_fw_event(wlog_verbose_level lev,
								 u16 fw_event_id,
								 u64 fw_timestamp,
								 void *fw_bulk_data,
								 size_t fw_blen)
{
	return 0;
}
#endif
