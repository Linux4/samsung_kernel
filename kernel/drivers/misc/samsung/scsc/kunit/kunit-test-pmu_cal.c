#include <kunit/test.h>
#include "../pmu_cal.h"
#include "../mif_reg.h"
#include "common.h"

struct pmucal_data KUNIT_TEST_SFR_FAIL[] = {
	{ WLBT_PMUCAL_WRITE, 1, 1, 0, 0, 0x0 },
};

struct pmucal_data KUNIT_TEST_SFR[] = {
	{ WLBT_PMUCAL_WRITE, 1, 1, WLBT_CTRL_NS, 0, 0x0 },
	{ WLBT_PMUCAL_WRITE, 1, 1, WLBT_CTRL_S, 0, 0x0 },
	{ WLBT_PMUCAL_WRITE, 1, 1, SYSTEM_OUT, 0, 0x0 },
	{ WLBT_PMUCAL_WRITE, 1, 1, WLBT_CONFIGURATION, 0, 0x0 },
	{ WLBT_PMUCAL_WRITE, 1, 1, WLBT_IN, 0, 0x0 },
	{ WLBT_PMUCAL_WRITE, 1, 1, VGPIO_TX_MONITOR, 0, 0x0 },
	{ WLBT_PMUCAL_WRITE, 1, 1, WLBT_INT_TYPE, 0, 0x0 },
	{ WLBT_PMUCAL_WRITE, 1, 1, WLBT_OPTION, 0, 0x0 },
	{ WLBT_PMUCAL_WRITE, 1, 1, V_PWREN, 0, 0x0 },
};
struct pmucal_data KUNIT_TEST_READ[] = {
	{ WLBT_PMUCAL_READ, 1, 1, VGPIO_TX_MONITOR, 31, 0x0 },
};
struct pmucal_data KUNIT_TEST_READ_FAIL_1[] = {
	{ WLBT_PMUCAL_READ, 1, 1, VGPIO_TX_MONITOR, 35, 0x0 },
};
struct pmucal_data KUNIT_TEST_READ_FAIL_2[] = {
	{ WLBT_PMUCAL_READ, 1, 1, VGPIO_TX_MONITOR, 31, 0xFFFFFFFE },
};

struct pmucal_data KUNIT_TEST_ATOMIC[] = {
	{ WLBT_PMUCAL_ATOMIC, 1, 1, WLBT_CTRL_NS, 0, 0x1 },
};

static void test_sfr(struct kunit *test)
{
	struct scsc_mif_abs *scscmif;
	struct platform_device pdev;
	struct device dev;
	struct platform_mif *platform;
	int ret;

	pdev.dev = dev;
	set_test_in_platform_mif(test);
	set_property(0x99);

	scscmif = platform_mif_create(&pdev);

	platform = container_of(scscmif, struct platform_mif, interface);

	ret = pmu_cal_progress(platform, KUNIT_TEST_SFR, 9);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = pmu_cal_progress(platform, KUNIT_TEST_SFR_FAIL, 1);
	KUNIT_EXPECT_EQ(test, ret, -22);
}

static void test_read(struct kunit *test)
{
	struct scsc_mif_abs *scscmif;
	struct platform_device pdev;
	struct device dev;
	struct platform_mif *platform;
	int ret;

	pdev.dev = dev;
	set_test_in_platform_mif(test);
	set_property(0x99);

	scscmif = platform_mif_create(&pdev);

	platform = container_of(scscmif, struct platform_mif, interface);

	ret = pmu_cal_progress(platform, KUNIT_TEST_READ, 1);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = pmu_cal_progress(platform, KUNIT_TEST_READ_FAIL_1, 1);
	KUNIT_EXPECT_EQ(test, ret, -22);
		ret = pmu_cal_progress(platform, KUNIT_TEST_READ_FAIL_2, 1);
	KUNIT_EXPECT_EQ(test, ret, -22);
}

static void test_atomic(struct kunit *test)
{
	struct scsc_mif_abs *scscmif;
	struct platform_device pdev;
	struct device dev;
	struct platform_mif *platform;
	int ret;

	pdev.dev = dev;
	set_test_in_platform_mif(test);
	set_property(0x99);

	scscmif = platform_mif_create(&pdev);

	platform = container_of(scscmif, struct platform_mif, interface);

	ret = pmu_cal_progress(platform, KUNIT_TEST_ATOMIC, 1);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static int test_init(struct kunit *test)
{
	return 0;
}

static void test_exit(struct kunit *test)
{
}

static struct kunit_case test_cases[] = {
	KUNIT_CASE(test_sfr),
	KUNIT_CASE(test_read),
	KUNIT_CASE(test_atomic),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_pmu_cal",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("youngss.kim@samsung.com>");
