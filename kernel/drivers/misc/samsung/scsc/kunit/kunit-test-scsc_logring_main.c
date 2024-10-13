#include <kunit/test.h>

extern int (*fp_logring_collect)(struct scsc_log_collector_client *collect_client, size_t size);
extern int (*fp_scsc_reset_all_droplevels_to_set_param_cb)(const char *val, const struct kernel_param *kp);

static void test_all(struct kunit *test)
{
	char *param;

	//samlog_init();

	//samlog_exit();

	fp_logring_collect(NULL, 0);

	fp_scsc_reset_all_droplevels_to_set_param_cb("+1", NULL);

	scsc_printk_tag_lvl(1, 1, NULL);

	scsc_printk_tag_dev_lvl(1, 6, NULL, 1, "kunit_test");

	param = "AAA";

	scsc_printk_bin(1, 1, 1, param, 3);

	scsc_logring_enable(true);

	scsc_logring_enable(false);

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
		.name = "test_kunit-test-scsc_logring_main.c",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

