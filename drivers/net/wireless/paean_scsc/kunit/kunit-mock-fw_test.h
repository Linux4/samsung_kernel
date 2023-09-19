/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_FW_TEST_H__
#define __KUNIT_MOCK_FW_TEST_H__

#include "../fw_test.h"

#define slsi_fw_test_signal_with_udi_header(args...)	kunit_mock_slsi_fw_test_signal_with_udi_header(args)
#define slsi_fw_test_deinit(args...)			kunit_mock_slsi_fw_test_deinit(args)
#define slsi_fw_test_init(args...)			kunit_mock_slsi_fw_test_init(args)
#define slsi_fw_test_signal(args...)			kunit_mock_slsi_fw_test_signal(args)


static int kunit_mock_slsi_fw_test_signal_with_udi_header(struct slsi_dev *sdev, struct slsi_fw_test *fwtest,
							  struct sk_buff *skb)
{
	return 0;
}

static void kunit_mock_slsi_fw_test_deinit(struct slsi_dev *sdev, struct slsi_fw_test *fwtest)
{
	return;
}

static void kunit_mock_slsi_fw_test_init(struct slsi_dev *sdev, struct slsi_fw_test *fwtest)
{
	return;
}

static int kunit_mock_slsi_fw_test_signal(struct slsi_dev *sdev, struct slsi_fw_test *fwtest, struct sk_buff *skb)
{
	return 0;
}
#endif
