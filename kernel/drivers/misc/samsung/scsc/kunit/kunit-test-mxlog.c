#include <kunit/test.h>
#include "../mxman.h"
#include "../scsc_mif_abs.h"
#include "../mxlog.h"

#define LOG_STRING_SIZE 256

struct message_set
{
	u8 phase;
	size_t length;
	u32 level;
	unsigned char data[100];
};

struct message_set _message_set[] =
{
	{MX_LOG_PHASE_5, 8,  5, "__log: ABCDE"},
	{MX_LOG_PHASE_5, 12, 5, "__log: ABCDE %d"},
	{MX_LOG_PHASE_5, 16, 5, "__log: ABCDE %d FGHIJK %d"},
	{MX_LOG_PHASE_5, 20, 5, "__log: ABCDE %d FGHIJK %d LMNOP %d"},
	{MX_LOG_PHASE_5, 24, 5, "__log: ABCDE %d FGHIJK %d LMNOP %d QRSTU %d"},
	{MX_LOG_PHASE_5, 28, 5, "__log: %% %%%% %lld %ll %hhd %s"},
	{MX_LOG_PHASE_5, 32, 5, "__log: ABCDE %d FGHIJK %d LMNOP %d QRSTU %d"},
	{MX_LOG_PHASE_5, 36, 5, "__log: ABCDE %d FGHIJK %d LMNOP %d QRSTU %d"},
	{MX_LOG_PHASE_5, 40, 5, "__log: ABCDE %d FGHIJK %d LMNOP %d QRSTU %d"},
	{MX_LOG_PHASE_5, 44, 5, "__Log: ABCDE %d FGHIJK %d LMNOP %d QRSTU %d ..."},
	{MX_LOG_PHASE_4, 24, 5, "__log: ABCDE %d FGHIJK %d LMNOP %d QRSTU %d"},
	{MX_LOG_PHASE_5, 8,  5, "__log: ABCDEFGHIJKLMNO"},
};

static void print_mxlog_data(struct mxlog *mxlog)
{
	char *fmt = (char *)(MXLS_DATA(mxlog));
	pr_info("mxlog data = %s\n", fmt);
}

static void mxlog_message_handler_sub_test(struct mxlog *mxlog, char data[], void *msg, int set_number)
{
	memset(data, 0, sizeof(data));
	strcpy(data, _message_set[set_number].data);
	mxlog_load_log_strings(mxlog, data, LOG_STRING_SIZE);

	kunit_mxlog_message_handler(
		_message_set[set_number].phase,
		msg,
		_message_set[set_number].length,
		_message_set[set_number].level,
		mxlog);

	//mxlog_unload_log_strings(mxlog);
}

static void test_mxlog_phase5_message_handler(struct kunit *test, struct mxlog *mxlog)
{
	struct mxlog_event_log_msg message;
	u32 data[10] = { 0x00000099, 0x00000002, 0x00000031, 0x00000041, 0x00000051, 0x00000061, 0x00000071, 0x00000081, 0x00000091 };
	message.timestamp = 0;
	message.offset = 0;

	mxlog_message_handler_sub_test(mxlog, data, (void *)(&message), 1);

	message.offset -= 1;
	mxlog_message_handler_sub_test(mxlog, data, (void *)(&message), 1);
}

static void test_all(struct kunit *test)
{
	struct scsc_mx *scscmx = test_alloc_scscmx(test, get_mif());
	struct mxman *mx = test_alloc_mxman(test);
	struct mxlog *mxlog = scsc_mx_get_mxlog(scscmx);
	struct mxlog_event_log_msg message;

	u32 level = 5;
	int logstrings_size = 256;
	char data[256];
	char build_id[128] = "BUILD_ID121234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678";
	int i;


	mx->mx = scscmx;

	strncpy(data, build_id, 128);

	mxlog_load_log_strings(mxlog, data, logstrings_size);
	mxlog_init(mxlog, scscmx, build_id, SCSC_MIF_ABS_TARGET_WLAN);
	kunit_mxlog_message_handler(MX_LOG_PHASE_4, NULL, 8, level, NULL);

	mxlog_unload_log_strings(mxlog);

	message.timestamp = 0;
	message.offset = 0;

	for (i = 0; i < sizeof(_message_set) / sizeof(struct message_set); i++)
		mxlog_message_handler_sub_test(mxlog, data, (void *)(&message), i);

	test_mxlog_phase5_message_handler(test, mxlog);
	mxlog_release(mxlog);
	strcpy(data, "WRONG_BUILD_ID");
	mxlog_load_log_strings(mxlog, data, 256);

	mxlog_init(mxlog, scscmx, "BUILD_ID", SCSC_MIF_ABS_TARGET_WPAN);


	mxlog_unload_log_strings(mxlog);

	mxlog_release(mxlog);

	strcpy(data, "WRONG_BUILD_ID");
	mxlog_load_log_strings(mxlog, NULL, 0);

	u32 header4 = 0xA55A0000;
	u32 header5 = 0x96690000;
	u32 lvl;
	u8 phase;
	u32 num_bytes;

	header4 |= 0x00000100;
	header4 |= 0x00000012;
	header5 |= 0x00000100;
	header5 |= 0x00000012;

	kunit_mxlog_header_parser(header4, &phase, &lvl, &num_bytes);
	kunit_mxlog_header_parser(header5, &phase, &lvl, &num_bytes);

	header5 |= 0x00001100;
	kunit_mxlog_header_parser(header5, &phase, &lvl, &num_bytes);

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
		.name = "test_mxlog",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

