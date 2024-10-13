#include <kunit/test.h>
#include <scsc/scsc_log_collector.h>

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
#define SCSC_NUM_CHUNKS_SUPPORTED	22
#else
#define SCSC_NUM_CHUNKS_SUPPORTED	13
#endif

extern int (*fp_sable_collection_off_set_param_cb)(const char *val, const struct kernel_param *kp);
extern int (*fp_sable_collection_off_get_param_cb)(const char *val, const struct kernel_param *kp);
extern int (*fp_scsc_log_collector_collect)(enum scsc_log_reason reason, u16 reason_code);

static bool sable_collection_off;
void set_sable_collection_off(bool val)
{
	sable_collection_off = val;
}

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
#include <scsc/scsc_log_collector.h>
#endif

extern void set_sable_collection_off(bool val);

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
#define SCSC_NUM_CHUNKS_SUPPORTED	22
#else
#define SCSC_NUM_CHUNKS_SUPPORTED	13
#endif

static void test_all(struct kunit *test)
{
	char buf[] = "initialisation";

	fp_sable_collection_off_set_param_cb("yes", NULL);

	fp_sable_collection_off_get_param_cb(buf, NULL);

	scsc_log_collector_exit();

	//fp_scsc_log_collector_compare(NULL, NULL, NULL);

	scsc_log_collector_unregister_mx_cb(NULL);
	scsc_log_collector_get_buffer();
	scsc_log_collector_write(NULL, 1, 0);
	scsc_log_collector_write_fapi(NULL, 0);

	/* abnormal case */
	struct scsc_log_collector_client *null_client;
	null_client = kunit_kzalloc(test, sizeof(*null_client), GFP_KERNEL);
	null_client->type = SCSC_NUM_CHUNKS_SUPPORTED;
	
	scsc_log_collector_register_client(null_client);

	scsc_log_collector_unregister_client(null_client);

	scsc_log_collector_write(NULL, SCSC_LOG_COLLECT_MAX_SIZE + 1, 0);

	set_sable_collection_off(true);
	fp_scsc_log_collector_collect(SCSC_LOG_USER, 0);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static int empty_collector_init(struct scsc_log_collector_client *collect_client){
	return 0;
}

static int dummy_collect(
	struct scsc_log_collector_client *collect_client,
	size_t size)
{
	return 0;
}

static struct scsc_log_collector_client dummy_client = {
	.name = "dummy",
	.type = SCSC_LOG_CHUNK_SYNC,
	.collect_init = empty_collector_init,
	.collect = dummy_collect,
	.collect_end = NULL,
	.prv = NULL,
};

static __init void register_collector_init(void)
{
	struct scsc_log_collector_client dummy_client;
	scsc_log_collector_register_client(&dummy_client);
}

static void test_exception(struct kunit *test)
{
	struct scsc_log_collector_client empty_client;
	int ret;
	int result;

	result = 0;
	empty_client.type = SCSC_LOG_CHUNK_INVALID;
	empty_client.name = kunit_kzalloc(test, sizeof(char) * 2, GFP_KERNEL);
	empty_client.name[0] = 'a';
	empty_client.name[1] = '\0';

	ret = scsc_log_collector_unregister_client(&empty_client);
	if (ret != 0) {
		 result = 1;
		 goto flag_test_exception_result;
	}

	ret = scsc_log_collector_register_client(&empty_client);
	if (ret != -EIO) {
		 result = 2;
		 goto flag_test_exception_result;
	}

flag_test_exception_result:
	KUNIT_EXPECT_EQ(test, 0, result);
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
	KUNIT_CASE(test_exception),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_scsc_log_collector",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

