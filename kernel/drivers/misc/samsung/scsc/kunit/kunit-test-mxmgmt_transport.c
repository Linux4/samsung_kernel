#include <kunit/test.h>
#include "../mxman.h"
#include "../mxmgmt_transport.h"

#define TH_RUN_DELAY		200	
#define TH_FUNC_RUN_TIME	50

extern void (*fp_mxmgmt_input_irq_handler)(int irq, void *data);
extern void (*fp_mxmgmt_input_irq_handler_wpan)(int irq, void *data);
extern void (*fp_mxmgmt_output_irq_handler)(int irq, void *data);
extern void (*fp_mxmgmt_output_irq_handler_wpan)(int irq, void *data);

void kunit_mxmgmt_channel_handler(const void *message, void *data)
{
}

static void test_mxmgmt_transport_wlan(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct mxmgmt_transport *mxmgmt_transport;

	mx = test_alloc_mxman(test);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;
	mxmgmt_transport = scsc_mx_get_mxmgmt_transport(scscmx);

	mxmgmt_transport_init(mxmgmt_transport, scscmx, SCSC_MIF_ABS_TARGET_WLAN);
	msleep(TH_RUN_DELAY);

	mxmgmt_transport_register_channel_handler(mxmgmt_transport, MMTRANS_CHAN_ID_MAXWELL_LOGGING,  &kunit_mxmgmt_channel_handler, NULL);

	fp_mxmgmt_input_irq_handler(0, mxmgmt_transport);
	msleep(TH_FUNC_RUN_TIME);

	fp_mxmgmt_output_irq_handler(0, mxmgmt_transport);

	mxmgmt_transport_send(mxmgmt_transport, MMTRANS_CHAN_ID_MAXWELL_LOGGING, "MESSAGE", 7);

	mxmgmt_transport_release(mxmgmt_transport);

	//kunit_mxmgmt_thread_stop(mxmgmt_transport);

	//kunit_thread_wait_until_stopped(mxmgmt_transport);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void test_mxmgmt_transport_wpan(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct mxmgmt_transport *mxmgmt_transport;

	mx = test_alloc_mxman(test);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;
	mxmgmt_transport = scsc_mx_get_mxmgmt_transport(scscmx);

	mxmgmt_transport_init(mxmgmt_transport, scscmx, SCSC_MIF_ABS_TARGET_WLAN);
	msleep(TH_RUN_DELAY);

	mxmgmt_transport_register_channel_handler(mxmgmt_transport, MMTRANS_CHAN_ID_MAXWELL_LOGGING,  &kunit_mxmgmt_channel_handler, NULL);

	fp_mxmgmt_input_irq_handler_wpan(0, mxmgmt_transport);
	msleep(TH_FUNC_RUN_TIME);

	fp_mxmgmt_output_irq_handler_wpan(0, mxmgmt_transport);

	mxmgmt_transport_send(mxmgmt_transport, MMTRANS_CHAN_ID_MAXWELL_LOGGING, "MESSAGE", 7);

	mxmgmt_transport_release(mxmgmt_transport);

	//kunit_mxmgmt_thread_stop(mxmgmt_transport);

	//kunit_thread_wait_until_stopped(mxmgmt_transport);

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
	KUNIT_CASE(test_mxmgmt_transport_wlan),
	KUNIT_CASE(test_mxmgmt_transport_wpan),

	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_mxmgmt_transport",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");


