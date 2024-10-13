#include <kunit/test.h>
#include "../mifmboxman.h"

static void test_all(struct kunit *test)
{
	struct mifmboxman *mbox;
	int first_mbox_index = 0;

	mbox = kunit_kzalloc(test, sizeof(*mbox), GFP_KERNEL);

	mifmboxman_init(mbox);

	mifmboxman_alloc_mboxes(mbox, 0, &first_mbox_index);
	mifmboxman_alloc_mboxes(mbox, 1, &first_mbox_index);

	mifmboxman_free_mboxes(mbox, 0, 0);
	mifmboxman_free_mboxes(mbox, 0, 1);

	mifmboxman_get_mbox_ptr(NULL, get_mif(), 0);

	mifmboxman_get_mbox_ptr_wpan(NULL, get_mif(), 0);

	mifmboxman_deinit(mbox);

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
		.name = "test_mifmboxman",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

