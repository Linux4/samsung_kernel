#include <kunit/test.h>
#include "../mxman.h"
#include "../srvman.h"
#include "../mxsyserr.h"
#include "../mxlog.h"

#define render_syserr_log(mxman, msg) kunit_render_syserr_log()

extern struct scsc_log_collector_mx_cb mx_cb;
int render_num;

int kunit_render_syserr_log(void)
{
	return render_num++;
}

static void test_all(struct kunit *test)
{
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct srvman srvman;
	struct srvman *srv;
	struct scsc_service *service;
	struct mx_syserr_msg message;
	struct mxlog *mxlog;
	int code;

	mx = test_alloc_mxman(test);
	scscmx = test_alloc_scscmx(test, get_mif());
	srvman.error = false;
	set_srvman(scscmx, srvman);
	mx->mx = scscmx;
	srv = scsc_mx_get_srvman(mx->mx);
	srvman_init(srv, scscmx);
	service = test_alloc_scscservice(test);
	list_add_service(service, srv);
	kunit_syserr_recovery_wq_init(mx);
	mx->last_syserr_recovery_time = 1;
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_register_mx_cb(&mx_cb);
#endif

	mx_syserr_init();

	u32 err_code = 0;
	mxlog = scsc_mx_get_mxlog(mx->mx);
	mxlog->logstrings = test_alloc_logstrings(test);
	MXLS_SZ(mxlog) = 30;
	MXLS_DATA(mxlog) = "%X1234%x%d%i%u%l%l0%i00i%%";
	code = MX_SYSERR_LEVEL_5;
	err_code |=  code << SYSERR_LEVEL_POSN;
	code = SYSERR_SUBSYS_WLAN;
	err_code |= code << SYSERR_SUB_SYSTEM_POSN;
	message.syserr.syserr_code = err_code;
	message.syserr.length = 1;
	message.syserr.slow_clock = 1;
	message.syserr.fast_clock = 1;
	message.syserr.string_index = 0;
	message.syserr.syserr_code = 1;
	message.syserr.param[0] = 1;
	message.syserr.param[1] = 1;

	mx_syserr_handler(mx, &message);

	message.syserr.string_index = 50;
	mx_syserr_handler(mx, &message);

	message.syserr.string_index = 4;
	mx_syserr_handler(mx, &message);

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
		.name = "test_mxsyserr",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

