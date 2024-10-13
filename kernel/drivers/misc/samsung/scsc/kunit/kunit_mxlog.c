#include <kunit/test.h>
#define srvman_forward_bt_fw_log(args...) (0)

struct mxlog_logstring  *test_alloc_logstrings(struct kunit *test)
{
	struct mxlog_logstring *logstrings;

	logstrings = kunit_kzalloc(test, sizeof(*logstrings), GFP_KERNEL);

	return logstrings;
}

static void mxlog_message_handler(u8 phase, const void *message, size_t length, u32 level, void *data);
void kunit_mxlog_message_handler(u8 phase, const void *message, size_t length, u32 level, void *data)
{
	return mxlog_message_handler(phase, message, length, level, data);
}

static int mxlog_header_parser(u32 header, u8 *phase, u8 *level, u32 *num_bytes);
int kunit_mxlog_header_parser(u32 header, u8 *phase, u8 *level, u32 *num_bytes)
{
	return mxlog_header_parser(header, phase, level, num_bytes);
}
