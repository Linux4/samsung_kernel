#include <kunit/test.h>
#include <../scsc_wlbtd.h>

extern int (*fp_msg_from_wlbtd_cb)(struct sk_buff *skb, struct genl_info *info);
extern int (*fp_msg_from_wlbtd_sable_cb)(struct sk_buff *skb, struct genl_info *info);
extern int (*fp_msg_from_wlbtd_build_type_cb)(struct sk_buff *skb, struct genl_info *info);
extern int (*fp_msg_from_wlbtd_write_file_cb)(struct sk_buff *skb, struct genl_info *info);

static void test_all(struct kunit *test)
{

	struct sk_buff *skb;
	struct genl_info *info;
	struct nlattr attrs[2];

	response_code_to_str(SCSC_WLBTD_ERR_PARSE_FAILED);
	response_code_to_str(SCSC_WLBTD_FW_PANIC_TAR_GENERATED);
	response_code_to_str(SCSC_WLBTD_FW_PANIC_ERR_SCRIPT_FILE_NOT_FOUND);
	response_code_to_str(SCSC_WLBTD_FW_PANIC_ERR_NO_DEV);
	response_code_to_str(SCSC_WLBTD_FW_PANIC_ERR_MMAP);
	response_code_to_str(SCSC_WLBTD_FW_PANIC_ERR_SABLE_FILE);
	response_code_to_str(SCSC_WLBTD_FW_PANIC_ERR_TAR);
	response_code_to_str(SCSC_WLBTD_OTHER_SBL_GENERATED);
	response_code_to_str(SCSC_WLBTD_OTHER_TAR_GENERATED);
	response_code_to_str(SCSC_WLBTD_OTHER_ERR_SCRIPT_FILE_NOT_FOUND);
	response_code_to_str(SCSC_WLBTD_OTHER_ERR_NO_DEV);
	response_code_to_str(SCSC_WLBTD_OTHER_ERR_MMAP);
	response_code_to_str(SCSC_WLBTD_OTHER_ERR_SABLE_FILE);
	response_code_to_str(SCSC_WLBTD_OTHER_ERR_TAR);
	response_code_to_str(SCSC_WLBTD_OTHER_IGNORE_TRIGGER);
	response_code_to_str(SCSC_WLBTD_LAST_RESPONSE_CODE);

	skb = nlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	info = kunit_kzalloc(test, sizeof(*info), GFP_KERNEL);
	info->attrs = &attrs;
	fp_msg_from_wlbtd_cb(&skb, &info);

	fp_msg_from_wlbtd_build_type_cb(&skb, NULL);
	// fp_msg_from_wlbtd_build_type_cb(&skb, &info);

	nla_put_string(skb, ATTR_STR, "Hi");
	nla_put_u32(skb, ATTR_CONTENT, 40);

	fp_msg_from_wlbtd_sable_cb(&skb, &info);

	scsc_wlbtd_get_and_print_build_type();

	wlbtd_write_file("ABC", "DEF");

	scsc_wlbtd_wait_for_sable_logging();

	call_wlbtd("ABD");

	scsc_wlbtd_deinit();

	kfree(skb);
	kfree(info);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void test_write_file_cb(struct kunit *test)
{

	struct sk_buff *skb;
	struct genl_info *info;
	struct nlattr attrs[2];

	skb = nlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	info = kunit_kzalloc(test, sizeof(*info), GFP_KERNEL);
	info->attrs = &attrs;

	nla_put_string(skb, ATTR_STR, "Hi");
	fp_msg_from_wlbtd_write_file_cb(&skb, &info);

	kfree(skb);
	kfree(info);

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
	KUNIT_CASE(test_write_file_cb),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_scsc_wlbtd",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

