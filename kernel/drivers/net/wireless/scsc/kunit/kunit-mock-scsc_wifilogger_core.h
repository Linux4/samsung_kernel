/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SCSC_WIFILOGGER_CORE_H__
#define __KUNIT_MOCK_SCSC_WIFILOGGER_CORE_H__

#include "../scsc_wifilogger_core.h"

#define scsc_wlog_flush_ring(args...)			kunit_mock_scsc_wlog_flush_ring(args)
#define scsc_wlog_ring_destroy(args...)			kunit_mock_scsc_wlog_ring_destroy(args)
#define scsc_wlog_ring_create(args...)			kunit_mock_scsc_wlog_ring_create(args)
#define scsc_wlog_drain_whole_ring(args...)		kunit_mock_scsc_wlog_drain_whole_ring(args)
#define scsc_wlog_register_loglevel_change_cb(args...)	kunit_mock_scsc_wlog_register_loglevel_change_cb(args)


static wifi_error kunit_test_get_ring_status(struct scsc_wlog_ring *r,
					     struct scsc_wifi_ring_buffer_status *status)
{
	if (!r || !status)
		return WIFI_ERROR_INVALID_ARGS;

	return WIFI_SUCCESS;
}

static int kunit_test_read_records(struct scsc_wlog_ring *r, u8 *buf,
				   size_t blen, u32 *records,
				   struct scsc_wifi_ring_buffer_status *status,
				   bool is_user)
{
	if (!r || !buf) return 0;
	return 1;
}

static int kunit_test_ring_drainer(struct scsc_wlog_ring *r, size_t drain_sz)
{
	return 0;
}

static int kunit_test_write_record(struct scsc_wlog_ring *r, u8 *buf, size_t blen,
				   void *ring_hdr, size_t hlen, u32 verbose_level, u64 timestamp)
{
	if (!r || !buf) return 0;
	return 1;
}

static wifi_error kunit_test_start_logging(struct scsc_wlog_ring *r, u32 verbose_level,
					   u32 flags, u32 max_interval_sec,
					   u32 min_data_size)
{
	if (!r) return WIFI_ERROR_INVALID_ARGS;
	return WIFI_SUCCESS;
}

static struct scsc_wlog_ring wr[2];

static void kunit_mock_scsc_wlog_flush_ring(struct scsc_wlog_ring *r)
{
	return;
}

static void kunit_mock_scsc_wlog_ring_destroy(struct scsc_wlog_ring *r)
{
	if (r) {
		if (r->initialized && r->ops.finalize)
			r->ops.finalize(r);
		r->initialized = false;
	}
	return;
}

static struct scsc_wlog_ring *kunit_mock_scsc_wlog_ring_create(char *ring_name, u32 flags,
							       u8 type, u32 size,
							       unsigned int features_mask,
							       init_cb init, finalize_cb fini,
							       void *priv)
{
	struct scsc_wlog_ring *r;

	if (!ring_name || !size)
		return NULL;

	if (strcmp(wr[0].st.name, "")) {
		if (strcmp(wr[1].st.name, ""))
			return NULL;
		r = &wr[1];
	} else {
		r = &wr[0];
	}

	r->type = type;
	r->st.flags = flags;
	r->st.rb_byte_size = size;
	snprintf(r->st.name, RING_NAME_SZ - 1, "%s", ring_name);

	r->ops.init = NULL,
	r->ops.finalize = NULL,
	r->ops.get_ring_status = kunit_test_get_ring_status,
	r->ops.read_records = kunit_test_read_records,
	r->ops.write_record = kunit_test_write_record,
	r->ops.loglevel_change = NULL,
	r->ops.drain_ring = kunit_test_ring_drainer,
	r->ops.start_logging = kunit_test_start_logging,

	r->ops.init = init;
	r->ops.finalize = fini;
	r->priv = priv;

	if (r->ops.init) {
		if (r->ops.init(r)) {
			r->initialized = true;
		} else {
			scsc_wlog_ring_destroy(r);
			return NULL;
		}
	}

	return r;
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
