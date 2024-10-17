#include <kunit/test.h>
#include "../mxman.h"

static void test_all(struct kunit *test)
{
	struct miframman *ramman;
	struct mxman *mx;
	struct scsc_mx *scscmx;
	void __iomem *dest;
	u32 img_size;
	char path = "ABC";

	mx = test_alloc_mxman(test);
	mx->start_dram = ((void *)0);
	mx->size_dram = (size_t)(91);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;

	set_test_in_mx140file(test);
	dest = kunit_kzalloc(test, 100, GFP_KERNEL);

	mx140_file_download_fw(scscmx, dest,  mx->size_dram, &img_size);

	//mx140_file_request_debug_conf(scscmx, NULL, NULL);

	mx140_file_select_fw(scscmx, 1);

	__mx140_release_firmware(scscmx, NULL);

	//__mx140_release_file(scscmx, NULL);

	mx140_exe_path(scscmx, &path, 1,  NULL);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void test_mx140_file_request_conf(struct kunit *test)
{
	struct miframman *ramman;
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct firmware *conf;

	mx = test_alloc_mxman(test);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;

	set_test_in_mx140file(test);

	mx140_file_request_conf(scscmx, &conf, "common", "common.hcf");
	KUNIT_EXPECT_STREQ(test, "OK", "OK");

}

static void test_mx140_file_request_debug_conf(struct kunit *test)
{
	struct miframman *ramman;
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct firmware *conf;

	mx = test_alloc_mxman(test);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;

	mx140_file_request_debug_conf(scscmx, &conf, "common/log-strings.bin");

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void test_mx140_file_request_device_conf(struct kunit *test)
{
	struct miframman *ramman;
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct firmware *conf;

	mx = test_alloc_mxman(test);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;

	mx140_file_request_device_conf(scscmx, &conf, "wlan/XXXXXX");

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
	KUNIT_CASE(test_mx140_file_request_conf),
	KUNIT_CASE(test_mx140_file_request_debug_conf),
	KUNIT_CASE(test_mx140_file_request_device_conf),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_mx140_file",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

