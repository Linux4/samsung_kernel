#include <kunit/test.h>
#include "../panicmon.h"

extern void (*fp_panicmon_isr)(int irq, void *data);

static void test_all(struct kunit *test)
{
	struct scsc_mx *scscmx;
	struct panicmon panicmon;

	scscmx = test_alloc_scscmx(test, get_mif());

	panicmon_init(&panicmon, scscmx);

	panicmon_deinit(&panicmon);

	fp_panicmon_isr(0, &panicmon);

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
		.name = "test_panicmon",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

