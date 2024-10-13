#include <kunit/test.h>
#include <scsc/scsc_mx.h>
#include "../mxman.h"
#include "../mifpmuman.h"

extern void (*fp_mifpmu_isr)(int irq, void *data);

static void test_all(struct kunit *test)
{
	struct mifpmuman pmu;
	struct mxman *mx;
	struct scsc_mx *scscmx;
	int ret;
	enum scsc_subsystem sub = SCSC_SUBSYSTEM_WLAN;

	mx = test_alloc_mxman(test);
	mx->start_dram = ((void *)0);
	mx->size_dram = (size_t)(0);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;

	pmu.in_use = false;

	mifpmuman_init(&pmu, get_mif(), NULL, NULL);

	mifpmuman_start_subsystem(&pmu, sub);

	mifpmuman_stop_subsystem(&pmu, sub);

	mifpmuman_force_monitor_mode_subsystem(&pmu, sub);

	sub = SCSC_SUBSYSTEM_WPAN;

	mifpmuman_start_subsystem(&pmu, sub);

	mifpmuman_stop_subsystem(&pmu, sub);

	mifpmuman_force_monitor_mode_subsystem(&pmu, sub);

	sub = SCSC_SUBSYSTEM_WLAN_WPAN;

	mifpmuman_start_subsystem(&pmu, sub);

	mifpmuman_stop_subsystem(&pmu, sub);

	mifpmuman_force_monitor_mode_subsystem(&pmu, sub);

	fp_mifpmu_isr(NULL, &pmu);

	mifpmuman_deinit(&pmu);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
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
		.name = "test_mifpmuman",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

