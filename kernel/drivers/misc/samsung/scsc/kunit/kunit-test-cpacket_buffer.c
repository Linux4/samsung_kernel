#include <kunit/test.h>
#include "../mxman.h"
#include "../cpacket_buffer.h"
#include "../miframman.h"
#include "../mxmgmt_transport_format.h"

static void test_all(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct cpacketbuffer cbuffer;
	struct miframman *miframman;
	char buf[1024];
	char buf1[1024];
	char buf2[1024];
	void *bufs[2] = { buf1, buf2};
	uint32_t lengths[2] = { 1024, 1024 };
	struct mxmgr_message current_message;

	mx = test_alloc_mxman(test);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;

	miframman = scsc_mx_get_ramman(scscmx);
	miframman->start_dram = ((void *)0x90000000);
	miframman->free_mem = 99999;
	miframman->num_blocks = 999;

	cpacketbuffer_init(&cbuffer, 4096, 3, scscmx);

	cpacketbuffer_write(&cbuffer, buf, 1024);

	cpacketbuffer_write_gather(&cbuffer, bufs, lengths, 2);

	cpacketbuffer_read(&cbuffer, buf, 1024);

	cpacketbuffer_peek(&cbuffer, &current_message);

	cpacketbuffer_peek_complete(&cbuffer, &current_message);

	cpacketbuffer_is_empty(&cbuffer);

	cpacketbuffer_is_full(&cbuffer);

	cpacketbuffer_free_space(&cbuffer);

	cpacketbuffer_packet_size(&cbuffer);

	cpacketbuffer_log(&cbuffer, 0);

	cpacketbuffer_release(&cbuffer);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static int test_init(struct kunit *test)
{
	return 0;
}

static void test_exit(struct kunit *test)
{
	return;
}

static struct kunit_case test_cases[] = {
	KUNIT_CASE(test_all),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_cpacket_buffer",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

