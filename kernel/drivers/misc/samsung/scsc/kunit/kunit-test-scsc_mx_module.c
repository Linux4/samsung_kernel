#include <kunit/test.h>
#include "common.h"
#include "../scsc_mif_abs.h"
#include "../scsc_mx_impl.h"

static void test_all(struct kunit *test)
{
	//kunit_scsc_mx_module_init();
	//kunit_scsc_mx_module_remove();
	//kunit_scsc_mx_module_exit();
	//scsc_mx_module_reset();
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
		.name = "test_scsc_mx_module",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("youngss.kim@samsung.com>");

