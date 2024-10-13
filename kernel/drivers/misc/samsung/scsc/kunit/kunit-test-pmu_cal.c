#include <kunit/test.h>
#include "../pmu_cal.h"
#include "../mif_reg.h"
#include "common.h"

#define regmap_update_bits(args...)	((void *)0)
#define regmap_read(args...)		((void *)0)

#define udelay(args...)			((void *)0)
#define regmap_write_bits(args...)	(0)

#define time_before(args...)		((bool)false)

struct pmucal_data KUNIT_TEST1[] = {
	{ WLBT_PMUCAL_WRITE, 1, 1, SYSTEM_OUT, 7, 0x0 },
	{ WLBT_PMUCAL_CLEAR, 1, 1, WLBT_IN, 0, 0x0 },
	{ WLBT_PMUCAL_READ, 1, 1, VGPIO_TX_MONITOR, 0, 0x0 },
	{ WLBT_PMUCAL_READ, 1, 1, VGPIO_TX_MONITOR2, 0, 0x0 },
	{ WLBT_PMUCAL_READ, 1, 1, WLBT_INT_TYPE, 0, 0x0 },
	{ WLBT_PMUCAL_READ, 1, 1, 15, 0, 0x0 },
	{ WLBT_PMUCAL_CLEAR, 1, 1, SYSTEM_OUT, 7, 0x0 },
};

struct pmucal_data KUNIT_TEST2[] = {
	{ WLBT_PMUCAL_READ, 1, 1, VGPIO_TX_MONITOR, 35, 0x0 },
};

struct pmucal_data KUNIT_TEST3[] = {
	{ WLBT_PMUCAL_READ, 1, 1, VGPIO_TX_MONITOR2, 0, 0xFFFFFFFE },
};

struct pmucal_data KUNIT_TEST4[] = {
	{ WLBT_PMUCAL_DELAY, 1, 0, 0, 0, 0x3 },
	{ WLBT_PMUCAL_CLEAR, 1, 1, WLBT_IN, 0, 0x0 },
	{ WLBT_PMUCAL_ATOMIC, 1, 1, WLBT_CTRL_NS, 31, 0x7 },
	{ WLBT_PMUCAL_WRITE, 1, 1, WLBT_OPTION, 3, 0x1 },
	{ WLBT_PMUCAL_WRITE, 1, 1, WLBT_CONFIGURATION, 0, 0x0 },
	{ WLBT_PMUCAL_READ, 1, 1, WLBT_STATUS, 0, 0x1 },
	{ WLBT_PMUCAL_READ, 1, 1, WLBT_IN, 4, 0x1 },
};

static void test_all(struct kunit *test)
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

	ret = pmu_cal_progress(platform, KUNIT_TEST1, 7);
	ret = pmu_cal_progress(platform, KUNIT_TEST2, 1);
	ret = pmu_cal_progress(platform, KUNIT_TEST3, 1);
	ret = pmu_cal_progress(platform, KUNIT_TEST4, 7);
}

static int test_init(struct kunit *test)
{
	return 0;
}

static void test_exit(struct kunit *test)
{
}

static struct kunit_case test_cases[] = {
	KUNIT_CASE(test_all),
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
