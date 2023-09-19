/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SAP_TEST_H__
#define __KUNIT_MOCK_SAP_TEST_H__

#include "../sap_test.h"

#define sap_test_init(args...)		kunit_mock_sap_test_init(args)
#define sap_test_deinit(args...)	kunit_mock_sap_test_deinit(args)


static int kunit_mock_sap_test_init(void)
{
	return 0;
}

static int kunit_mock_sap_test_deinit(void)
{
	return 0;
}
#endif
