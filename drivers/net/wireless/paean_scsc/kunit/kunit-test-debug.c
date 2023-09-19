#include <kunit/test.h>

#include "../debug.c"

/* test */
static void test_slsi_decstr_to_int(struct kunit *test)
{
	char dec_str[10] = "343\n";
	int res;

	KUNIT_EXPECT_EQ(test, 0, slsi_decstr_to_int(dec_str, &res));
}

static void test_slsi_dbg_set_param_cb(struct kunit *test)
{
	char val[10] = "343\n";
	struct kernel_param *kp = kunit_kzalloc(test, sizeof(struct kernel_param), GFP_KERNEL);
	int filter = SLSI_OVERRIDE_ALL_FILTER;

	kp->arg = &filter;
	KUNIT_EXPECT_EQ(test, 0, slsi_dbg_set_param_cb(val, kp));
	filter = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_dbg_set_param_cb(val, kp));
	filter = -3;
	KUNIT_EXPECT_EQ(test, -1, slsi_dbg_set_param_cb(val, kp));
	val[0] = 'a';
	KUNIT_EXPECT_EQ(test, -1, slsi_dbg_set_param_cb(val, kp));
}

static void test_slsi_dbg_get_param_cb(struct kunit *test)
{
	char val[10] = "343\n";
	struct kernel_param *kp = kunit_kzalloc(test, sizeof(struct kernel_param), GFP_KERNEL);
	int filter = SLSI_OVERRIDE_ALL_FILTER;

	kp->arg = &filter;
	KUNIT_EXPECT_EQ(test, 0, slsi_dbg_set_param_cb(val, kp));
	filter = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_dbg_set_param_cb(val, kp));
	filter = -3;
	KUNIT_EXPECT_EQ(test, -1, slsi_dbg_set_param_cb(val, kp));
}

/* Test fictures*/
static int debug_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void debug_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case debug_test_cases[] = {
	/* debug.c */
	KUNIT_CASE(test_slsi_decstr_to_int),
	KUNIT_CASE(test_slsi_dbg_set_param_cb),
	KUNIT_CASE(test_slsi_dbg_get_param_cb),
	{}
};

static struct kunit_suite debug_test_suite[] = {
	{
		.name = "kunit-debug-test",
		.test_cases = debug_test_cases,
		.init = debug_test_init,
		.exit = debug_test_exit,
	}
};

kunit_test_suites(debug_test_suite);
