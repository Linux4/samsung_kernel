#include <kunit/test.h>
#include "../mxman.h"
#include "../mifstream.h"
#include "../miframman.h"
#include "../scsc_mif_abs.h"
#include "../mifintrbit.h"
#include "../mxmgmt_transport_format.h"

static void test_all(struct kunit *test)
{
	struct mif_stream stream;
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct miframman *miframman;
	struct mifintrbit *intr;
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

	intr = scsc_mx_get_intrbit(scscmx);
	mifintrbit_init(intr, get_mif(), SCSC_MIF_ABS_TARGET_WLAN);

	mif_stream_init(&stream, SCSC_MIF_ABS_TARGET_WLAN, MIF_STREAM_DIRECTION_IN, 4096, 3, scscmx, MIF_STREAM_INTRBIT_TYPE_ALLOC, NULL, NULL, 0);

	mif_stream_write(&stream, buf, 1024);

	mif_stream_write_gather(&stream, bufs, lengths, 2);

	mif_stream_read(&stream, buf, 1024);

	mif_stream_peek(&stream, &current_message);

	mif_stream_peek_complete(&stream, &current_message);

	mif_stream_block_size(&stream);

	mif_stream_read_interrupt(&stream);

	mif_stream_write_interrupt(&stream);

	mif_stream_log(&stream, 0);

	mif_stream_release(&stream);

	mif_stream_init(&stream, SCSC_MIF_ABS_TARGET_WLAN, MIF_STREAM_DIRECTION_OUT, 1, 1, scscmx, MIF_STREAM_INTRBIT_TYPE_ALLOC, NULL, NULL, 0);
	mif_stream_release(&stream);
	mif_stream_init(&stream, SCSC_MIF_ABS_TARGET_WLAN, 2, 1, 1, scscmx, MIF_STREAM_INTRBIT_TYPE_ALLOC, NULL, NULL, 0);
	mif_stream_release(&stream);

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
		.name = "test_mifstream",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

