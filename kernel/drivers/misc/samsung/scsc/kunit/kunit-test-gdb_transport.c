#include <kunit/test.h>
#include "../mxman.h"
#include "../gdb_transport.h"

extern void (*fp_gdb_input_irq_handler)(int irq, void *data);
extern void (*fp_gdb_output_irq_handler)(int irq, void *data);

void gdb_client_probe(struct gdb_transport_client *gdb_client, struct gdb_transport *gdb_transport, char *dev_uid)
{
}
void gdb_client_remove(struct gdb_transport_client *gdb_client, struct gdb_transport *gdb_transport)
{
}

static struct gdb_transport_client client_gdb_driver = {
	.name = "GDB client driver",
	.probe = gdb_client_probe,
	.remove = gdb_client_remove,
};

void kunit_gdb_channel_handler(u8 phase, const void *message, size_t length, u32 level, void *data)
{
}

static void test_gdb_transport_wlan(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct gdb_transport *gdb_transport;

	mx = test_alloc_mxman(test);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;
	gdb_transport = scsc_mx_get_gdb_transport_wlan(scscmx);

	gdb_transport_register_client(&client_gdb_driver);

	gdb_transport_init(gdb_transport, scscmx, GDB_TRANSPORT_WLAN);

	gdb_transport_register_channel_handler(gdb_transport, &kunit_gdb_channel_handler, 0);

	fp_gdb_input_irq_handler(0, gdb_transport);

	fp_gdb_output_irq_handler(0, gdb_transport);

	gdb_transport_send(gdb_transport, "MESSAGE", 7);

	gdb_transport_unregister_client(&client_gdb_driver);

	gdb_transport_release(gdb_transport);

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
	KUNIT_CASE(test_gdb_transport_wlan),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_gdb_transport",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");


