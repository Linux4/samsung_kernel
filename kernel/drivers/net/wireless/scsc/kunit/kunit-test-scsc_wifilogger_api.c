// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-misc.h"
#include "kunit-mock-scsc_wifilogger_internal.h"
#include "kunit-mock-scsc_wifilogger_ring_pktfate.h"
#include "kunit-mock-scsc_wifilogger_core.h"
#include "../scsc_wifilogger_api.c"

static void test_on_ring_buffer_data_cb(char *ring_name, char *buf, int bsz,
					struct scsc_wifi_ring_buffer_status *status,
					void *ctx)
{
	printk("%s called.\n", __func__);
}

static void test_on_alert_cb(char *buffer, int buffer_size, int err_code, void *ctx)
{
	printk("%s called.\n", __func__);
}

static void test_on_firmware_memory_dump_cb(char *buffer, int buffer_size, int err_code, void *ctx)
{
	printk("%s called.\n", __func__);
}

static void test_on_driver_memory_dump_cb(char *buffer, int buffer_size, int err_code, void *ctx)
{
	printk("%s called.\n", __func__);
}

/* unit test function definition */
static void test_scsc_wifi_get_firmware_version(struct kunit *test)
{
	char buffer[128] = {0};
	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_get_firmware_version(buffer, sizeof(buffer)));
}

static void test_scsc_wifi_get_driver_version(struct kunit *test)
{
	char buffer[64] = {0};
	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_get_driver_version(buffer, sizeof(buffer)));
}

static void test_scsc_wifi_get_ring_buffers_status(struct kunit *test)
{
	int num_rings = 0;
	struct scsc_wifi_ring_buffer_status status[10] = {0,};

	KUNIT_EXPECT_EQ(test, WIFI_ERROR_UNINITIALIZED, scsc_wifi_get_ring_buffers_status(&num_rings, &status));

	num_rings = 10;
	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_get_ring_buffers_status(&num_rings, &status));
}

static void test_scsc_wifi_get_logger_supported_feature_set(struct kunit *test)
{
	unsigned int support = 0;
	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_get_logger_supported_feature_set(&support));
}

static void test_scsc_wifi_set_log_handler(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	wifi_logger.initialized = false;
	KUNIT_EXPECT_EQ(test, WIFI_ERROR_UNINITIALIZED, scsc_wifi_set_log_handler(NULL, sdev));

	wifi_logger.initialized = true;
	KUNIT_EXPECT_EQ(test, WIFI_ERROR_INVALID_ARGS, scsc_wifi_set_log_handler(NULL, sdev));

	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_set_log_handler(test_on_ring_buffer_data_cb, sdev));

	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_set_log_handler(test_on_ring_buffer_data_cb, sdev));
}

static void test_scsc_wifi_reset_log_handler(struct kunit *test)
{
	wifi_logger.initialized = false;
	KUNIT_EXPECT_EQ(test, WIFI_ERROR_UNINITIALIZED, scsc_wifi_reset_log_handler());

	wifi_logger.initialized = true;
	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_reset_log_handler());
}

static void test_scsc_wifi_set_alert_handler(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	wifi_logger.initialized = false;
	KUNIT_EXPECT_EQ(test, WIFI_ERROR_UNINITIALIZED, scsc_wifi_set_alert_handler(NULL, sdev));

	wifi_logger.initialized = true;
	KUNIT_EXPECT_EQ(test, WIFI_ERROR_INVALID_ARGS, scsc_wifi_set_alert_handler(NULL, sdev));

	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_set_alert_handler(test_on_alert_cb, sdev));

	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_set_alert_handler(test_on_alert_cb, sdev));
}

static void test_scsc_wifi_reset_alert_handler(struct kunit *test)
{
	wifi_logger.initialized = false;
	KUNIT_EXPECT_EQ(test, WIFI_ERROR_UNINITIALIZED, scsc_wifi_reset_alert_handler());

	wifi_logger.initialized = true;
	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_reset_alert_handler());
}

static void test_scsc_wifi_get_ring_data(struct kunit *test)
{
	char *ring_name = "invalid_name";

	wifi_logger.initialized = false;
	KUNIT_EXPECT_EQ(test, WIFI_ERROR_UNINITIALIZED, scsc_wifi_get_ring_data(ring_name));

	wifi_logger.initialized = true;
	KUNIT_EXPECT_EQ(test, WIFI_ERROR_NOT_AVAILABLE, scsc_wifi_get_ring_data(ring_name));

	ring_name = "valid_name";
	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_get_ring_data(ring_name));
}

static void test_scsc_wifi_start_logging(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, WIFI_ERROR_NOT_AVAILABLE, scsc_wifi_start_logging(1, 0x00, 0, 8192, "invalid_name"));

	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_start_logging(1, 0x00, 0, 8192, "valid_name"));
}

static void test_scsc_wifi_get_firmware_memory_dump(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_get_firmware_memory_dump(test_on_firmware_memory_dump_cb, sdev));
}

static void test_scsc_wifi_get_driver_memory_dump(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_get_driver_memory_dump(test_on_driver_memory_dump_cb, sdev));
}

static void test_scsc_wifi_start_pkt_fate_monitoring(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_start_pkt_fate_monitoring());
}

static void test_scsc_wifi_get_tx_pkt_fates(struct kunit *test)
{
	void __user *user_buf = NULL;
	size_t provided_fates = 0;
	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_get_tx_pkt_fates(user_buf, 0, &provided_fates, true));
}

static void test_scsc_wifi_get_rx_pkt_fates(struct kunit *test)
{
	void __user *user_buf = NULL;
	size_t provided_fates = 0;
	KUNIT_EXPECT_EQ(test, WIFI_SUCCESS, scsc_wifi_get_rx_pkt_fates(user_buf, 0, &provided_fates, true));
}

/* Test fictures */
static int scsc_wifilogger_api_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void scsc_wifilogger_api_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case scsc_wifilogger_api_test_cases[] = {
	KUNIT_CASE(test_scsc_wifi_get_firmware_version),
	KUNIT_CASE(test_scsc_wifi_get_driver_version),
	KUNIT_CASE(test_scsc_wifi_get_ring_buffers_status),
	KUNIT_CASE(test_scsc_wifi_get_logger_supported_feature_set),
	KUNIT_CASE(test_scsc_wifi_set_log_handler),
	KUNIT_CASE(test_scsc_wifi_reset_log_handler),
	KUNIT_CASE(test_scsc_wifi_set_alert_handler),
	KUNIT_CASE(test_scsc_wifi_reset_alert_handler),
	KUNIT_CASE(test_scsc_wifi_get_ring_data),
	KUNIT_CASE(test_scsc_wifi_start_logging),
	KUNIT_CASE(test_scsc_wifi_get_firmware_memory_dump),
	KUNIT_CASE(test_scsc_wifi_get_driver_memory_dump),
	KUNIT_CASE(test_scsc_wifi_start_pkt_fate_monitoring),
	KUNIT_CASE(test_scsc_wifi_get_tx_pkt_fates),
	KUNIT_CASE(test_scsc_wifi_get_rx_pkt_fates),
	{}
};

static struct kunit_suite scsc_wifilogger_api_test_suite[] = {
	{
		.name = "scsc_wifilogger_api-test",
		.test_cases = scsc_wifilogger_api_test_cases,
		.init = scsc_wifilogger_api_test_init,
		.exit = scsc_wifilogger_api_test_exit,
	}
};

kunit_test_suites(scsc_wifilogger_api_test_suite);
