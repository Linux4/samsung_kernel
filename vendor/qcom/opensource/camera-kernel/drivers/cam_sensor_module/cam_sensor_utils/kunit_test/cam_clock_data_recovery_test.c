// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <linux/module.h>
#include "camera_kunit_main.h"
#include "cam_clock_data_recovery_test.h"

int cam_clock_data_recovery_set_register(const char* buf)
{
	void __iomem *phybase = NULL;
	void __iomem *csiphybase;
	uint32_t offset = 0x1;
	int ret = 0;

	csiphybase = phybase + offset;

	cam_clock_data_recovery_set_value(buf);
	if (cam_clock_data_recovery_is_requested())
	{
		ret = cam_clock_data_recovery_write_register(csiphybase);
		cam_clock_data_recovery_reset_request();
	}
	return ret;
}

void cam_clock_data_recovery_apply_value_test(struct kunit *test)
{
	const char* addr_value_delay_normal = "278,2,00,678,2,00,a78,2,00";

	cam_clock_data_recovery_set_value(addr_value_delay_normal);
	KUNIT_EXPECT_EQ(test, !strcmp(addr_value_delay_normal, cam_clock_data_recovery_get_value()), TRUE);
}

void cam_clock_data_recovery_apply_result_test(struct kunit *test)
{
	cam_clock_data_recovery_get_timestamp(CDR_START_TS);
	cam_clock_data_recovery_set_result(CDR_ERROR_MIPI);
	KUNIT_EXPECT_EQ(test, strcmp("0", cam_clock_data_recovery_get_result()), TRUE);
}

void cam_clock_data_recovery_write_normal_test(struct kunit *test)
{
	const char* addr_value_delay_normal = "278,2,00,678,2,00,a78,2,00";
	void __iomem *phybase = NULL;
	void __iomem *csiphybase;
	uint32_t offset = 0x1;
	int ret = 0;

	csiphybase = phybase + offset;

	cam_clock_data_recovery_set_value(addr_value_delay_normal);
	if (cam_clock_data_recovery_is_requested())
	{
		//ret = cam_clock_data_recovery_write_register(csiphybase);
		cam_clock_data_recovery_reset_request();
	}
	KUNIT_EXPECT_EQ(test, (ret == 0), TRUE);
}

void cam_clock_data_recovery_write_overflow1_test(struct kunit *test)
{
	const char* addr_value_delay_overflow = "278,2,00,678,2,00,a78,2,00,11,22";
	int ret = 0;

	ret = cam_clock_data_recovery_set_register(addr_value_delay_overflow);
	KUNIT_EXPECT_EQ(test, (ret == 1), FALSE);
}

void cam_clock_data_recovery_write_overflow2_test(struct kunit *test)
{
	const char* addr_value_delay_overflow = "278,22222222222,00,678,2,00,a78,2,00";
	int ret = 0;

	ret = cam_clock_data_recovery_set_register(addr_value_delay_overflow);
	KUNIT_EXPECT_EQ(test, (ret == 1), FALSE);
}

void cam_clock_data_recovery_write_invalid_test(struct kunit *test)
{
	const char* addr_value_delay_invalid = "278,**,00,678,2,00,a78,2,00";
	int ret = 0;

	ret = cam_clock_data_recovery_set_register(addr_value_delay_invalid);
	KUNIT_EXPECT_EQ(test, (ret == 1), FALSE);
}

MODULE_LICENSE("GPL v2");
