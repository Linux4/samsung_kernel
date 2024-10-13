/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_UDI_H__
#define __KUNIT_MOCK_UDI_H__

#include "../udi.h"

#define slsi_udi_node_deinit(args...)			kunit_mock_slsi_udi_node_deinit(args)
#define slsi_udi_deinit(args...)			kunit_mock_slsi_udi_deinit(args)
#define slsi_check_cdev_refs(args...)			kunit_mock_slsi_check_cdev_refs(args)
#define slsi_udi_node_init(args...)			kunit_mock_slsi_udi_node_init(args)
#define slsi_kernel_to_user_space_event(args...)	kunit_mock_slsi_kernel_to_user_space_event(args)
#define slsi_udi_init(args...)				kunit_mock_slsi_udi_init(args)


static int kunit_mock_slsi_udi_node_deinit(struct slsi_dev *sdev)
{
	return 0;
}

static int kunit_mock_slsi_udi_deinit(void)
{
	return 0;
}

static int kunit_mock_slsi_check_cdev_refs(void)
{
	return 0;
}

static int kunit_mock_slsi_udi_node_init(struct slsi_dev *sdev, struct device *parent)
{
	if (!sdev || !sdev->maxwell_core)
		return -EINVAL;

	return 0;
}

static int kunit_mock_slsi_kernel_to_user_space_event(struct slsi_log_client *log_client,
						      u16 event, u32 data_length, const u8 *data)
{
	return 0;
}

static int kunit_mock_slsi_udi_init(void)
{
	return 0;
}
#endif
