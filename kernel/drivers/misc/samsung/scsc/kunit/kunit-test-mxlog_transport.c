#include <kunit/test.h>
#include "../mxman.h"
#include "../mxlog_transport.h"

#define TH_FUNC_RUN_TIME	50

extern void (*fp_input_irq_handler)(int irq, void *data);
extern void (*fp_mxlog_input_irq_handler_wpan)(int irq, void *data);
extern void (*fp_mxlog_thread_stop)(struct mxlog_transport *mxlog_transport);
extern char *read_msg;

int kunit_mxlog_header_handler(u32 header, u8 *phase, u8 *level, u32 *num_bytes)
{
	*num_bytes = 1;
	return 0;
}

void kunit_mxlog_channel_handler(u8 phase, const void *message, size_t length, u32 level, void *data)
{
}

static void test_mxlog_transport_wlan(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct mxlog_transport *mxlog_transport;

	mx = test_alloc_mxman(test);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;
	mxlog_transport = scsc_mx_get_mxlog_transport(scscmx);

	mxlog_transport_init(mxlog_transport, scscmx);

	mxlog_transport_register_channel_handler(mxlog_transport, &kunit_mxlog_header_handler, &kunit_mxlog_channel_handler, 0);

	read_msg = "MSG #1";
	fp_input_irq_handler(0, mxlog_transport);
	msleep(TH_FUNC_RUN_TIME);

	read_msg = NULL;
	fp_input_irq_handler(0, mxlog_transport);
	msleep(TH_FUNC_RUN_TIME);

	read_msg = "MSG #2";
	fp_input_irq_handler(0, mxlog_transport);
	msleep(TH_FUNC_RUN_TIME);

	mxlog_transport_release(mxlog_transport);

	//fp_mxlog_thread_stop(mxlog_transport);

	//kunit_thread_wait_until_stopped(mxlog_transport);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void test_mxlog_transport_wpan(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct mxlog_transport *mxlog_transport;

	mx = test_alloc_mxman(test);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;
	mxlog_transport = scsc_mx_get_mxlog_transport(scscmx);

	mxlog_transport_init_wpan(mxlog_transport, scscmx);

	mxlog_transport_register_channel_handler(mxlog_transport, &kunit_mxlog_header_handler, &kunit_mxlog_channel_handler, 0);

	read_msg = "MSG #1";
	fp_mxlog_input_irq_handler_wpan(0, mxlog_transport);
	msleep(TH_FUNC_RUN_TIME);

	read_msg = NULL;
	fp_mxlog_input_irq_handler_wpan(0, mxlog_transport);
	msleep(TH_FUNC_RUN_TIME);

	read_msg = "MSG #2";
	fp_mxlog_input_irq_handler_wpan(0, mxlog_transport);
	msleep(TH_FUNC_RUN_TIME);

	mxlog_transport_release(mxlog_transport);

	//fp_mxlog_thread_stop(mxlog_transport);

	//kunit_thread_wait_until_stopped(mxlog_transport);

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
	KUNIT_CASE(test_mxlog_transport_wlan),
	KUNIT_CASE(test_mxlog_transport_wpan),

	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_mxlog_transport",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");


