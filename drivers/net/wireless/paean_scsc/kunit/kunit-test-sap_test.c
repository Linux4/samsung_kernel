// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-rx.h"
#include "kunit-mock-hip.h"
#include "../sap_test.c"

static void test_sap_test_version_supported(struct kunit *test)
{
	u16 version = FAPI_TEST_SAP_VERSION;
	KUNIT_EXPECT_EQ(test, 0, sap_test_version_supported(version));

	version = FAPI_DEBUG_SAP_VERSION;
	KUNIT_EXPECT_EQ(test, -EINVAL, sap_test_version_supported(version));
}

static void test_sap_test_rx_handler(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, sap_test_rx_handler(sdev, NULL));
	KUNIT_EXPECT_EQ(test, 0, sap_test_rx_handler(sdev, skb));
}

static void test_sap_test_init(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, sap_test_init());
}

static void test_sap_test_deinit(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, sap_test_deinit());
}

/* Test fictures */
static int sap_test_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void sap_test_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case sap_test_test_cases[] = {
	KUNIT_CASE(test_sap_test_version_supported),
	KUNIT_CASE(test_sap_test_rx_handler),
	KUNIT_CASE(test_sap_test_init),
	KUNIT_CASE(test_sap_test_deinit),
	{}
};

static struct kunit_suite sap_test_test_suite[] = {
	{
		.name = "sap_test-test",
		.test_cases = sap_test_test_cases,
		.init = sap_test_test_init,
		.exit = sap_test_test_exit,
	}
};

kunit_test_suites(sap_test_test_suite);
