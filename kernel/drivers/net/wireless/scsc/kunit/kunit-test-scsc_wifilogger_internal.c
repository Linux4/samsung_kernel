// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-misc.h"
#include "kunit-mock-scsc_wifilogger_debugfs.h"
#include "../scsc_wifilogger_internal.c"

static wifi_error __get_ring_status(struct scsc_wlog_ring *r,
				    struct scsc_wifi_ring_buffer_status *status)
{
	printk("%s called.\n", __func__);
	return WIFI_SUCCESS;
}

static void __on_alert_cb(char *buffer, int buffer_size, int err_code, void *ctx)
{
	printk("%s called.\n", __func__);
}

/* unit test function definition */
static void test_scsc_wifilogger_get_handle(struct kunit *test)
{
	scsc_wifilogger_get_handle();
}

static void test_scsc_wifilogger_init(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, scsc_wifilogger_init());
}

static void test_scsc_wifilogger_notifier(struct kunit *test)
{
	struct scsc_wifi_logger *wl = scsc_wifilogger_get_handle();
	struct notifier_block *nb = NULL;
	unsigned long event = SCSC_FW_EVENT_FAILURE;
	char *data = "";

	wl->initialized = false;
	KUNIT_EXPECT_EQ(test, NOTIFY_BAD, scsc_wifilogger_notifier(nb, event, (void *)data));

	wl->initialized = true;
	event = 999;
	KUNIT_EXPECT_EQ(test, NOTIFY_BAD, scsc_wifilogger_notifier(nb, event, (void *)data));

	event = SCSC_FW_EVENT_FAILURE;
	KUNIT_EXPECT_EQ(test, NOTIFY_DONE, scsc_wifilogger_notifier(nb, event, (void *)data));

	event = SCSC_FW_EVENT_MOREDUMP_COMPLETE;
	wl->on_alert_cb = __on_alert_cb;
	KUNIT_EXPECT_EQ(test, NOTIFY_OK, scsc_wifilogger_notifier(nb, event, (void *)data));
}

static void test_scsc_wifilogger_fw_alert_init(struct kunit *test)
{
	struct scsc_wifi_logger *wl = scsc_wifilogger_get_handle();

	wl->initialized = false;
	KUNIT_EXPECT_FALSE(test, scsc_wifilogger_fw_alert_init());

	wl->initialized = true;
	KUNIT_EXPECT_TRUE(test, scsc_wifilogger_fw_alert_init());
}

static void test_scsc_wifilogger_get_features(struct kunit *test)
{
	struct scsc_wifi_logger *wl = scsc_wifilogger_get_handle();

	wl->initialized = false;
	KUNIT_EXPECT_EQ(test, 0, scsc_wifilogger_get_features());

	wl->initialized = true;
	KUNIT_EXPECT_NE(test, 0, scsc_wifilogger_get_features());
}

#define SLSI_MAX_NUM_RING 10
static void test_scsc_wifilogger_get_rings_status(struct kunit *test)
{
	struct scsc_wifi_logger *wl = scsc_wifilogger_get_handle();
	int num_rings = SLSI_MAX_NUM_RING;
	struct scsc_wifi_ring_buffer_status status[SLSI_MAX_NUM_RING];
	memset(status, 0, sizeof(struct scsc_wifi_ring_buffer_status) * num_rings);

	wl->initialized = false;
	KUNIT_EXPECT_FALSE(test, scsc_wifilogger_get_rings_status(&num_rings, status));

	num_rings = SLSI_MAX_NUM_RING;
	wl->initialized = true;
	memset(wl->rings, 0, sizeof(struct scsc_wlog_ring) * MAX_WIFI_LOGGER_RINGS);
	wl->rings[0] = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);
	wl->rings[0]->ops.get_ring_status = __get_ring_status;
	wl->rings[0]->registered = true;
	KUNIT_EXPECT_TRUE(test, scsc_wifilogger_get_rings_status(&num_rings, status));
}

static void test_scsc_wifilogger_get_ring_from_name(struct kunit *test)
{
	char *name = "test_logring";
	struct scsc_wifi_logger *wl = scsc_wifilogger_get_handle();

	wl->initialized = false;
	KUNIT_EXPECT_PTR_EQ(test, NULL, (struct scsc_wlog_ring *)scsc_wifilogger_get_ring_from_name(name));

	wl->initialized = true;
	wl->rings[0] = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);
	strncpy(wl->rings[0]->st.name, name, RING_NAME_SZ);
	wl->rings[0]->initialized = true;
	wl->rings[0]->registered = true;
	KUNIT_EXPECT_PTR_NE(test, NULL, (struct scsc_wlog_ring *)scsc_wifilogger_get_ring_from_name(name));
}

static void test_scsc_wifilogger_register_ring(struct kunit *test)
{
	struct scsc_wlog_ring *r = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);
	struct scsc_wifi_logger *wl = scsc_wifilogger_get_handle();
	if (wl) wl->initialized = false;
	KUNIT_EXPECT_FALSE(test, scsc_wifilogger_register_ring(r));

	if (wl) {
		wl->initialized = true;
		memset(wl->rings, 0, sizeof(struct scsc_wlog_ring) * MAX_WIFI_LOGGER_RINGS);
	}
	r->st.ring_id = 0;
	KUNIT_EXPECT_TRUE(test, scsc_wifilogger_register_ring(r));

	if (wl) {
		wl->rings[0] = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);
	}
	KUNIT_EXPECT_FALSE(test, scsc_wifilogger_register_ring(r));
}

static void test_scsc_wifilogger_destroy(struct kunit *test)
{
	struct scsc_wifi_logger *wl = scsc_wifilogger_get_handle();
	wl->initialized = false;
	scsc_wifilogger_destroy();

	wl->initialized = true;
	memset(wl->rings, 0, sizeof(struct scsc_wlog_ring) * MAX_WIFI_LOGGER_RINGS);
	wl->rings[0] = kunit_kzalloc(test, sizeof(struct scsc_wlog_ring), GFP_KERNEL);
	wl->rings[0]->initialized = true;
	scsc_wifilogger_destroy();
}


/* Test fictures */
static int scsc_wifilogger_internal_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void scsc_wifilogger_internal_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case scsc_wifilogger_internal_test_cases[] = {
	KUNIT_CASE(test_scsc_wifilogger_get_handle),
	KUNIT_CASE(test_scsc_wifilogger_init),
	KUNIT_CASE(test_scsc_wifilogger_notifier),
	KUNIT_CASE(test_scsc_wifilogger_fw_alert_init),
	KUNIT_CASE(test_scsc_wifilogger_get_features),
	KUNIT_CASE(test_scsc_wifilogger_get_rings_status),
	KUNIT_CASE(test_scsc_wifilogger_get_ring_from_name),
	KUNIT_CASE(test_scsc_wifilogger_register_ring),
	KUNIT_CASE(test_scsc_wifilogger_destroy),
	{}
};

static struct kunit_suite scsc_wifilogger_internal_test_suite[] = {
	{
		.name = "scsc_wifilogger_internal-test",
		.test_cases = scsc_wifilogger_internal_test_cases,
		.init = scsc_wifilogger_internal_test_init,
		.exit = scsc_wifilogger_internal_test_exit,
	}
};

kunit_test_suites(scsc_wifilogger_internal_test_suite);
