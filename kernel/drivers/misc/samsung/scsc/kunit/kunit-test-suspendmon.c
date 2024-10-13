#include <kunit/test.h>
#include "common.h"

extern int (*fp_suspendmon_suspend)(struct scsc_mif_abs *mif, void *data);
extern void (*fp_suspendmon_resume)(struct scsc_mif_abs *mif, void *data);


static void test_all(struct kunit *test)
{
	struct suspendmon *suspendmon;
	struct scsc_mx *scscmx;
	struct srvman *srvman;
	struct mxman *mxman;

	mxman = kunit_kzalloc(test, sizeof(*mxman), GFP_KERNEL);
	srvman = kunit_kzalloc(test, sizeof(*srvman), GFP_KERNEL);
	suspendmon = kunit_kzalloc(test, sizeof(*suspendmon), GFP_KERNEL);
	scscmx = test_alloc_scscmx(test, get_mif());
	srvman_init(srvman, scscmx);
	srvman->error = false;
	mxman->mx = scscmx;
	mutex_init(&mxman->mxman_mutex);
	set_srvman(scscmx, srvman);
	set_mxman(scscmx, mxman);
	suspendmon->mx = scscmx;

	//suspendmon_init(suspendmon, scscmx);
	fp_suspendmon_suspend(get_mif(), suspendmon);
	fp_suspendmon_resume(get_mif(), suspendmon);
	suspendmon_deinit(suspendmon);

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
		.name = "test_suspendmon",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("youngss.kim@samsung.com>");

