#include <kunit/test.h>

static void test_all(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, "OK", "OK");
	KUNIT_EXPECT_STREQ(test, "EXPECTATION", "RESULT");
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
		.name = "test_filename", <--- change filename
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>"); <--- change author

