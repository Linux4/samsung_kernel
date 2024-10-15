#include <kunit/test.h>

#include "../platform_mif.h"
#include <linux/platform_device.h>

static void test_manual_pmu_reset_assert(struct kunit *test)
{
	int ret;
	ret = platform_mif_manual_pmu_reset_assert(get_mif());
	KUNIT_EXPECT_EQ(test, ret, -1);
}

static void test_manual_pmu_reset_release(struct kunit *test)
{
	int ret;
	int *dummy;
	ret = manual_pmu_reset_release(get_mif(), dummy);
	KUNIT_EXPECT_EQ(test, ret, -1);
}

static int test_init(struct kunit *test)
{
	return 0;
}

static void test_exit(struct kunit *test)
{
}

static struct kunit_case test_cases[] = {
	KUNIT_CASE(test_manual_pmu_reset_assert),
	KUNIT_CASE(test_manual_pmu_reset_release),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_s5e8845_legacy",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("youngss.kim@samsung.com>");

