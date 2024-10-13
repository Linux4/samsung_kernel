#include <kunit/test.h>

extern int (*fp_scsc_lerna_chardev_open)(struct inode *inodep, struct file *filep);
extern ssize_t (*fp_scsc_lerna_chardev_read)(struct file *filep, char *buffer, size_t len, loff_t *offset);
extern ssize_t (*fp_scsc_lerna_chardev_write)(struct file *filep, const char *buffer, size_t len, loff_t *offset);
extern int (*fp_scsc_lerna_chardev_release)(struct inode *inodep, struct file *filep);

static void test_all(struct kunit *test)
{
	int data[4] = {0x000c1308, 0x21222324, 0x31323334, 0x41424344};
	char buffer[1024];
	struct scsc_lerna_cmd_header *header;

	fp_scsc_lerna_chardev_open(NULL, NULL);

	header = test_alloc_header(test);
	set_header(header);

	fp_scsc_lerna_chardev_read(NULL, buffer, 1024, NULL);

	fp_scsc_lerna_chardev_write(NULL, NULL, 0, NULL);

	fp_scsc_lerna_chardev_release(NULL, NULL);

	scsc_lerna_init();

	scsc_lerna_deinit();

	scsc_lerna_response(data);

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
		.name = "test_scsc_lerna",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

