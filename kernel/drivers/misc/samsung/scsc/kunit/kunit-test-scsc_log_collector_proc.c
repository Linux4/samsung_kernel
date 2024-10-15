#include <kunit/test.h>

#include "../logs/scsc_log_collector_proc.h"

extern int (*fp_log_collect_procfs_open_file_generic)(struct inode *inode, struct file *file);
extern ssize_t (*fp_log_collect_procfs_trigger_collection_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_log_collect_procfs_trigger_collection_write)(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos);

static void test_all(struct kunit *test)
{
	struct file file;
	struct inode inode;
	char user_buf;
	int arg_len = 2;
	loff_t ppos = 1;

	fp_log_collect_procfs_open_file_generic(&inode, &file);

	fp_log_collect_procfs_trigger_collection_read(NULL, &user_buf, 1, &ppos);

	user_buf = '1';

	fp_log_collect_procfs_trigger_collection_write(NULL, &user_buf, arg_len, NULL);

	user_buf = '2';

	fp_log_collect_procfs_trigger_collection_write(NULL, &user_buf, arg_len, NULL);

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
		.name = "test_scsc_log_collector_proc",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

