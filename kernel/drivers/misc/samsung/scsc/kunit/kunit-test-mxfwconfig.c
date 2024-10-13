#include <kunit/test.h>
#include "../mxman.h"
#include "../mxfwconfig.h"

extern int (*fp_mxfwconfig_load_cfg)(struct scsc_mx *mx, struct mxfwconfig *cfg, const char *filename);

static void test_all(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct mxmibref cfg_ref;

	mx = test_alloc_mxman(test);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;

	set_test_in_mxfwconfig(test);

	mxfwconfig_init(scscmx);

	cfg_ref.offset = 0;
	cfg_ref.size = 0;

	mxfwconfig_load(scscmx, &cfg_ref);

	mxfwconfig_unload(scscmx);

	mxfwconfig_deinit(scscmx);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void test_mxfwconfig_load_cfg(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;

	mx = test_alloc_mxman(test);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;

	set_test_in_mxfwconfig(test);
	fp_mxfwconfig_load_cfg(scscmx, scsc_mx_get_mxfwconfig(scscmx), "common.hcf");

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
	KUNIT_CASE(test_mxfwconfig_load_cfg),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_mxfwconfig",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

