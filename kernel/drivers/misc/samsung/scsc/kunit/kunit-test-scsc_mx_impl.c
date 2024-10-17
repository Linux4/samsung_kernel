#include <kunit/test.h>
#include "common.h"
#include "../mxman.h"
#include "../fwhdr_if.h"
#include "../mxfwconfig.h"
#include "../srvman.h"

static void test_all(struct kunit *test)
{
	struct scsc_mif_abs mif;
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct srvman srvman;
	struct mifintrbit intr;
	struct mifintrbit intr_wpan;

	mx = test_alloc_mxman(test);
	mx->start_dram = ((void *)0);
	mx->size_dram = (size_t)(0);
	mx->scsc_panic_code = 1;
	scscmx = test_alloc_scscmx(test, get_mif());

	srvman.error = false;
	srvman_init(&srvman, scscmx);
	set_srvman(scscmx, srvman);
	mx->mx = scscmx;
	mx->mxman_state = MXMAN_STATE_STARTING;

	set_mxman(scscmx, mx);
	scscmx->intr = intr;
	scscmx->intr_wpan = intr_wpan;
	//scsc_mx_create(get_mif());
	//scsc_mx_destroy(scscmx);
	scsc_mx_get_mif_abs(scscmx);
	scsc_mx_get_intrbit(scscmx);
	scsc_mx_get_mifpmuman(scscmx);
	scsc_mx_get_ramman(scscmx);
	scsc_mx_get_ramman2(scscmx);
	scsc_mx_get_aboxram(scscmx);
	scsc_mx_get_mboxman(scscmx);
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	scsc_mx_get_ramman2_wpan(scscmx);
	scsc_mx_get_ramman_wpan(scscmx);
	scsc_mx_get_mboxman_wpan(scscmx);
	scsc_mx_get_mxmgmt_transport_wpan(scscmx);
	scsc_mx_get_gdb_transport_wpan(scscmx);
	scsc_mx_get_mxlog_wpan(scscmx);
	scsc_mx_get_mxlog_transport_wpan(scscmx);
#endif
#ifdef CONFIG_SCSC_SMAPPER
	scsc_mx_get_smapper(scscmx);
#endif
#ifdef CONFIG_SCSC_QOS
	scsc_mx_get_qos(scscmx);
#endif
	scsc_mx_get_device(scscmx);
	scsc_mx_get_mxman(scscmx);
	scsc_mx_get_srvman(scscmx);
	scsc_mx_get_mxmgmt_transport(scscmx);
	scsc_mx_get_gdb_transport_wlan(scscmx);
	scsc_mx_get_gdb_transport_fxm_1(scscmx);
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	scsc_mx_get_gdb_transport_fxm_2(scscmx);
#endif
	scsc_mx_get_mxlog(scscmx);
	scsc_mx_get_panicmon(scscmx);
	scsc_mx_get_mxlog_transport(scscmx);
	scsc_mx_get_mxlogger(scscmx);
	scsc_mx_get_suspendmon(scscmx);
	scsc_mx_get_mxfwconfig(scscmx);
	scsc_mx_request_firmware_mutex_lock(scscmx);
	scsc_mx_request_firmware_mutex_unlock(scscmx);
	scsc_mx_request_firmware_wake_lock(scscmx);
	scsc_mx_request_firmware_wake_unlock(scscmx);

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
		.name = "test_scsc_mx_impl",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("youngss.kim@samsung.com>");

