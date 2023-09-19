// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-kernel.h"
#include "../scsc_wifilogger_core.c"


static void __on_ring_buffer_data_cb(char *ring_name, char *buf, int bsz,
		 struct scsc_wifi_ring_buffer_status *status,
		 void *ctx)
{
	printk("%s called.\n", __func__);
}

static int __loglevel_change_cb(struct scsc_wlog_ring *r,
				u32 new_loglevel)
{
	printk("%s called.\n", __func__);
	printk("loglevel changed for ring: %s -- to:%d\n",
		  r->st.name, new_loglevel);
	return 0;
}

bool __ring_init_success_cb(struct scsc_wlog_ring *r)
{
	printk("%s called for ring:%s\n", __func__, r->st.name);
	return true;
}

bool __ring_init_failure_cb(struct scsc_wlog_ring *r)
{
	printk("%s called for ring:%s\n", __func__, r->st.name);
	return false;
}

bool __ring_finalize_cb(struct scsc_wlog_ring *r)
{
	printk("%s called for ring:%s\n", __func__, r->st.name);
	return true;
}

static int __wlog_read_records(struct scsc_wlog_ring *r, u8 *buf,
			       size_t blen, u32 *records,
			       struct scsc_wifi_ring_buffer_status *status,
			       bool is_user)
{
	printk("%s called.\n", __func__);
	return 1024;
}

static int __wlog_write_record(struct scsc_wlog_ring *r, u8 *buf, size_t blen,
			       void *ring_hdr, size_t hlen, u32 verbose_level, u64 timestamp)
{
	printk("%s called.\n", __func__);
	return 0;
}

static int __wlog_ring_drainer(struct scsc_wlog_ring *r, size_t drain_sz)
{
	printk("%s called.\n", __func__);
	return 0;
}

static struct scsc_wlog_ring_ops test_ring_ops = {
	.init = __ring_init_success_cb,
	.finalize = __ring_finalize_cb,
	.get_ring_status = NULL,
	.read_records = __wlog_read_records,
	.write_record = __wlog_write_record,
	.loglevel_change = __loglevel_change_cb,
	.drain_ring = __wlog_ring_drainer,
	.start_logging = NULL,
};

/* unit test function definition */
static void test_wlog_drain_worker(struct kunit *test)
{
	struct scsc_wlog_ring *r = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);
	r->ops.drain_ring = __wlog_ring_drainer;

	wlog_drain_worker(&r->drain_work);
}

#if KERNEL_VERSION(4, 19, 0) <= LINUX_VERSION_CODE
static void test_drain_timer_callback(struct kunit *test)
{
	struct timer_list *t = kunit_kzalloc(test, sizeof(struct timer_list), GFP_KERNEL);
	drain_timer_callback(t);
}
#else
static void test_drain_timer_callback(struct kunit *test)
{
	unsigned long data;
	drain_timer_callback(data);
}
#endif

static void test_wlog_ring_init(struct kunit *test)
{
	struct scsc_wlog_ring *r = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);
	wlog_ring_init(r);

	kfree(r->buf);
	kfree(r->drain_buf);
}


static void test_wlog_ring_finalize(struct kunit *test)
{
	struct scsc_wlog_ring *r = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);

	r->drain_buf = kzalloc(DRAIN_BUF_SZ, GFP_KERNEL);
	r->buf = kzalloc(DRAIN_BUF_SZ, GFP_KERNEL);
	wlog_ring_finalize(r);
}

static void test_wlog_get_ring_status(struct kunit *test)
{
	struct scsc_wlog_ring *r = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);
	struct scsc_wifi_ring_buffer_status status[10];

	KUNIT_EXPECT_EQ(test, wlog_get_ring_status(r, status), WIFI_SUCCESS);
}

static void test_wlog_read_records(struct kunit *test)
{
	struct scsc_wlog_ring *r = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);
	size_t blen = 4096;
	u8 *buf = kunit_kzalloc(test, blen, GFP_KERNEL);
	u32 records = 512;
	struct scsc_wifi_ring_buffer_status status[10];

	r->buf = kunit_kzalloc(test, blen + MAX_RECORD_SZ, GFP_KERNEL);
	r->st.written_bytes = 1024;
	r->st.read_bytes = 128;

	KUNIT_EXPECT_NE(test, -1, wlog_read_records(r, buf, blen, &records, status, true));
	KUNIT_EXPECT_NE(test, -1, wlog_read_records(r, buf, blen, &records, status, false));

	r->flushing = true;

	KUNIT_EXPECT_EQ(test, 0, wlog_read_records(r, buf, blen, &records, status, false));
}

static void test_wlog_default_ring_drainer(struct kunit *test)
{
	struct scsc_wlog_ring *r = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);
	size_t drain_sz = DRAIN_BUF_SZ;

	memcpy(&r->ops, &test_ring_ops, sizeof(struct scsc_wlog_ring_ops));
	r->wl = kunit_kzalloc(test, sizeof(struct scsc_wifi_logger), GFP_KERNEL);
	r->wl->on_ring_buffer_data_cb = __on_ring_buffer_data_cb;
	r->flushing = false;
	KUNIT_EXPECT_NE(test, -1, wlog_default_ring_drainer(r, drain_sz));

	r->flushing = true;
	KUNIT_EXPECT_NE(test, -1, wlog_default_ring_drainer(r, drain_sz));
}

static void test_wlog_write_record(struct kunit *test)
{
	struct scsc_wlog_ring *r = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);
	size_t blen = MAX_RECORD_SZ;
	u8 *buf = kunit_kzalloc(test, blen, GFP_KERNEL);
	size_t hlen = 1024;
	void *ring_hdr = kunit_kzalloc(test, hlen, GFP_KERNEL);
	u32 verbose_level = WLOG_NORMAL;
	u64 timestamp = 0;

	r->st.verbose_level = WLOG_DEBUG;
	KUNIT_EXPECT_EQ(test, wlog_write_record(r, buf, blen, ring_hdr, hlen, verbose_level, timestamp), 0);

	blen = 4096;
	KUNIT_EXPECT_EQ(test, wlog_write_record(r, buf, blen, ring_hdr, hlen, verbose_level, timestamp), 0);

	r->st.rb_byte_size = blen + hlen + MAX_RECORD_SZ;
	r->buf = kunit_kzalloc(test, r->st.rb_byte_size, GFP_KERNEL);
	KUNIT_EXPECT_GT(test, wlog_write_record(r, buf, blen, ring_hdr, hlen, verbose_level, timestamp), 0);
}

static void test_wlog_default_ring_config_change(struct kunit *test)
{
	struct scsc_wlog_ring *r = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);
	u32 verbose_level = 1;
	u32 flags = 1;
	u32 max_interval_sec = 2;
	u32 min_data_size = 8192;

	r->state = RING_STATE_SUSPEND;
	r->st.verbose_level = 1;
	KUNIT_EXPECT_EQ(test, 0,
			wlog_default_ring_config_change(r, verbose_level, flags,
							max_interval_sec, min_data_size));

	r->state = RING_STATE_ACTIVE;
	KUNIT_EXPECT_EQ(test, 0,
			wlog_default_ring_config_change(r, verbose_level, flags,
							max_interval_sec, min_data_size));
	
	r->st.verbose_level = 0;
	KUNIT_EXPECT_EQ(test, 0,
			wlog_default_ring_config_change(r, verbose_level, flags,
							max_interval_sec, min_data_size));
}

static void test_wlog_start_logging(struct kunit *test)
{
	struct scsc_wlog_ring *r = NULL;
	u32 verbose_level = 0;
	u32 flags = 0;
	u32 max_interval_sec = 0;
	u32 min_data_size = 8192;

	KUNIT_EXPECT_EQ(test, WIFI_ERROR_INVALID_ARGS,
			wlog_start_logging(r, verbose_level, flags, max_interval_sec, min_data_size));

	r = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);
	r->st.verbose_level = 1;
	r->ops.loglevel_change = __loglevel_change_cb;
	r->verbosity = kunit_kzalloc(test, sizeof(u32), GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS,
			wlog_start_logging(r, verbose_level, flags, max_interval_sec, min_data_size));
}

static void test_scsc_wlog_ring_destroy(struct kunit *test)
{
	struct scsc_wlog_ring *r = NULL;
	scsc_wlog_ring_destroy(r);

	r = kzalloc(sizeof(struct scsc_wlog_ring), GFP_KERNEL);
	r->drain_buf = kzalloc(DRAIN_BUF_SZ, GFP_KERNEL);
	r->buf = kzalloc(DRAIN_BUF_SZ, GFP_KERNEL);
	r->initialized = true;
	r->ops.finalize = __ring_finalize_cb;
	scsc_wlog_ring_destroy(r);
}

static void test_scsc_wlog_ring_create(struct kunit *test)
{
	char *ring_name = "test_logring";
	u32 flags = RING_BUFFER_ENTRY_FLAGS_HAS_BINARY;
	u8 type = ENTRY_TYPE_CONNECT_EVENT;
	u32 size = 32768 * 8;
	unsigned int features_mask = WIFI_LOGGER_CONNECT_EVENT_SUPPORTED;
	init_cb init = __ring_init_failure_cb;
	finalize_cb fini = __ring_finalize_cb;
	void *priv = NULL;
	struct scsc_wlog_ring *res;
	struct scsc_wlog_ring *r = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);
	struct scsc_wifi_ring_buffer_status status;

	res = scsc_wlog_ring_create(ring_name, flags, type, size, features_mask, init, fini, priv);

	KUNIT_EXPECT_PTR_EQ(test, res, NULL);

	kfree(res);

	init = __ring_init_success_cb;
	res = scsc_wlog_ring_create(ring_name, flags, type, size, features_mask, init, fini, priv);

	KUNIT_EXPECT_PTR_NE(test, res, NULL);

	r->st.verbose_level = 1;
	r->ops.loglevel_change = __loglevel_change_cb;
	r->verbosity = kunit_kzalloc(test, sizeof(u32), GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, res->ops.start_logging(r, 0, 0, 0, 8192));

	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, res->ops.get_ring_status(r, &status));

	kfree(res->buf);
	kfree(res->drain_buf);
	kfree(res);

}

static void test_scsc_wlog_register_loglevel_change_cb(struct kunit *test)
{
	struct scsc_wlog_ring *r = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, scsc_wlog_register_loglevel_change_cb(r, NULL), 0);
	KUNIT_EXPECT_EQ(test, scsc_wlog_register_loglevel_change_cb(r, __loglevel_change_cb), 0);
}

static void test_scsc_wlog_drain_whole_ring(struct kunit *test)
{
	struct scsc_wlog_ring *r = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);
	sprintf(r->st.name, "test_logring\n");
	memcpy(&r->ops, &test_ring_ops, sizeof(struct scsc_wlog_ring_ops));
	KUNIT_EXPECT_NE(test, -1, scsc_wlog_drain_whole_ring(r));
}

static void test_scsc_wlog_flush_ring(struct kunit *test)
{
	struct scsc_wlog_ring *r = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);

	scsc_wlog_flush_ring(r);
}


/* Test fictures */
static int scsc_wifilogger_core_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void scsc_wifilogger_core_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case scsc_wifilogger_core_test_cases[] = {
	KUNIT_CASE(test_wlog_ring_init),
	KUNIT_CASE(test_scsc_wlog_ring_create),
	KUNIT_CASE(test_wlog_drain_worker),
	KUNIT_CASE(test_drain_timer_callback),
	KUNIT_CASE(test_wlog_start_logging),
	KUNIT_CASE(test_wlog_get_ring_status),
	KUNIT_CASE(test_wlog_read_records),
	KUNIT_CASE(test_wlog_default_ring_drainer),
	KUNIT_CASE(test_wlog_write_record),
	KUNIT_CASE(test_wlog_default_ring_config_change),
	KUNIT_CASE(test_scsc_wlog_register_loglevel_change_cb),
	KUNIT_CASE(test_scsc_wlog_drain_whole_ring),
	KUNIT_CASE(test_scsc_wlog_flush_ring),
	KUNIT_CASE(test_wlog_ring_finalize),
	KUNIT_CASE(test_scsc_wlog_ring_destroy),
	{}
};

static struct kunit_suite scsc_wifilogger_core_test_suite[] = {
	{
		.name = "scsc_wifilogger_core-test",
		.test_cases = scsc_wifilogger_core_test_cases,
		.init = scsc_wifilogger_core_test_init,
		.exit = scsc_wifilogger_core_test_exit,
	}
};

kunit_test_suites(scsc_wifilogger_core_test_suite);
