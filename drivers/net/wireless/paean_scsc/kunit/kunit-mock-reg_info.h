/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_REG_INFO_H__
#define __KUNIT_MOCK_REG_INFO_H__

#include "../reg_info.h"

#define slsi_read_regulatory(args...)			kunit_mock_slsi_read_regulatory(args)
#define slsi_regd_deinit(args...)			kunit_mock_slsi_regd_deinit(args)
#define slsi_regd_init_wiphy_not_registered(args...)	kunit_mock_slsi_regd_init_wiphy_not_registered(args)
#define slsi_regd_init(args...)				kunit_mock_slsi_regd_init(args)


static int kunit_mock_slsi_read_regulatory(struct slsi_dev *sdev)
{
	if (sdev && sdev->regdb.regdb_state == SLSI_REG_DB_SET) return 0;
	return -EINVAL;
}

static void kunit_mock_slsi_regd_deinit(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_slsi_regd_init_wiphy_not_registered(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_slsi_regd_init(struct slsi_dev *sdev)
{
	return;
}
#endif
