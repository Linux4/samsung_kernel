#include <kunit/test.h>
#include "../miframman.h"

static void test_all(struct kunit *test)
{
	struct miframman *ram;
	struct seq_file *fd;
	struct mifabox mifabox;

	miframabox_init(&mifabox, NULL);

	miframabox_deinit(&mifabox);

	ram = kunit_kzalloc(test, sizeof(*ram), GFP_KERNEL);
	fd = kunit_kzalloc(test, sizeof(*fd), GFP_KERNEL);

	ram->num_blocks = 2;
	ram->bitmap[0] = 0x12;
	ram->bitmap[1] = 0x41;

	miframman_log(ram, fd);

	ram->bitmap[1] = 0x11;
	miframman_log(ram, fd);

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
		.name = "test_miframman",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

