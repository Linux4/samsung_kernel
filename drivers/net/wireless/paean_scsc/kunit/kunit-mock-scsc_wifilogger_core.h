/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SCSC_WIFILOGGER_CORE_H__
#define __KUNIT_MOCK_SCSC_WIFILOGGER_CORE_H__

#include "../scsc_wifilogger_core.h"

#define scsc_wlog_flush_ring(args...)			kunit_mock_scsc_wlog_flush_ring(args)
#define scsc_wlog_ring_destroy(args...)			kunit_mock_scsc_wlog_ring_destroy(args)
#define scsc_wlog_ring_create(args...)			kunit_mock_scsc_wlog_ring_create(args)
#define scsc_wlog_drain_whole_ring(args...)		kunit_mock_scsc_wlog_drain_whole_ring(args)
#define scsc_wlog_register_loglevel_change_cb(args...)	kunit_mock_scsc_wlog_register_loglevel_change_cb(args)


static int kunit_mock_read_records(struct scsc_wlog_ring *r, u8 *buf, size_t blen, u32 *records,
				   struct scsc_wifi_ring_buffer_status *status, bool is_user)
{
	if (!r || !buf) return 0;
	return 1;
}

static int kunit_mock_write_record(struct scsc_wlog_ring *r, u8 *buf, size_t blen, void *ring_hdr,
				   size_t hlen, u32 verbose_level, u64 timestamp)
{
	if (!r || !buf) return 0;
	return 1;
}

static wifi_error kunit_mock_start_logging(struct scsc_wlog_ring *r, u32 verbose_level,
					   u32 flags, u32 max_interval_sec,
					   u32 min_data_size)
{
	if (!r) return WIFI_ERROR_INVALID_ARGS;
	return WIFI_SUCCESS;
}

static struct scsc_wlog_ring wr;

static void kunit_mock_scsc_wlog_flush_ring(struct scsc_wlog_ring *r)
{
	return;
}

static void kunit_mock_scsc_wlog_ring_destroy(struct scsc_wlog_ring *r)
{
	return;
}

static struct scsc_wlog_ring *kunit_mock_scsc_wlog_ring_create(char *ring_name, u32 flags,
							       u8 type, u32 size,
							       unsigned int features_mask,
							       init_cb init, finalize_cb fini,
							       void *priv)
{
	if (!ring_name || !size)
		return NULL;

	wr.ops.read_records = kunit_mock_read_records;
	wr.ops.write_record = kunit_mock_write_record;
	wr.ops.start_logging = kunit_mock_start_logging;
	return &wr;
}

static int kunit_mock_scsc_wlog_drain_whole_ring(struct scsc_wlog_ring *r)
{
	return 0;
}

static int kunit_mock_scsc_wlog_register_loglevel_change_cb(struct scsc_wlog_ring *r,
							    int (*callback)(struct scsc_wlog_ring *r, u32 new_loglevel))

{
	return 0;
}
#endif
