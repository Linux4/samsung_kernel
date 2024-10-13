#include <kunit/test.h>
#include <scsc/scsc_mx.h>
#include "../mxman.h"
#include "../srvman.h"
#include "common.h"

extern u8 (*fp_test_failure_notification)(struct scsc_service_client *client, struct mx_syserr_decode *err);
extern bool (*fp_test_stop_on_failure)(struct scsc_service_client *client, struct mx_syserr_decode *err);
extern void (*fp_test_failure_reset)(struct scsc_service_client *client, u8 level, u16 scsc_syserr_code);
extern int (*fp_client_test_dev_open)(struct inode *inode, struct file *file);
extern ssize_t (*fp_client_test_dev_write)(struct file *file, const char *data, size_t len, loff_t *offset);
extern ssize_t (*fp_client_test_dev_read)(struct file *filp, char *buffer, size_t length, loff_t *offset);
extern int (*fp_client_test_dev_release)(struct inode *inode, struct file *file);
extern int (*fp_scsc_client_test_module_init)(void);
extern void (*fp_scsc_client_test_module_exit)(void);
extern void (*fp_delay_start_func)(struct work_struct *work);

static void test_client_test_all(struct kunit *test)
{
	struct mx_syserr_decode syserr;
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct fwhdr_if *fw_if;
	char *data = "TESTDATA";
	size_t data_sz = sizeof(data);
	struct work_struct *work;

	enum scsc_subsystem sub = SCSC_SUBSYSTEM_WLAN;

	mx = test_alloc_mxman(test);
	mx->start_dram = ((void *)0);
	mx->size_dram = (size_t)(0);
	mx->scsc_panic_code = 1;
	scscmx = test_alloc_scscmx(test, get_mif());

	scscmx->srvman.error = false;
	srvman_init(&scscmx->srvman, scscmx);

	mx->mx = scscmx;
	mx->mxman_state = MXMAN_STATE_STOPPED;
	fw_if = test_alloc_fwif(test);
	mx->fw_wlan = fw_if;

	set_mxman(scscmx, mx);
	set_auto_start(1);		// 1: auto_start, 2:delayed start
	set_service_id_2(1);

	client_module_probe(NULL, scscmx, 0);

	client_module_remove(NULL, scscmx, 0);

	scsc_log_in_dram_class = NULL;

	fp_client_test_dev_open(NULL, NULL);

	fp_client_test_dev_write(NULL, "1", 1, NULL);
	fp_client_test_dev_write(NULL, "0", 1, NULL);

	fp_client_test_dev_read(NULL, NULL, 0, NULL);

	fp_client_test_dev_release(NULL, NULL);

	fp_scsc_client_test_module_init();

	fp_scsc_client_test_module_exit();

	fp_test_failure_notification(NULL, &syserr);

	fp_test_stop_on_failure(NULL, NULL);

	fp_test_failure_reset(NULL, 0, 0);

	set_auto_start(2);		// 1: auto_start, 2:delayed start
	set_service_id_2(-1);

	client_module_probe(NULL, scscmx, 0);

	client_module_remove(NULL, scscmx, 0);

	scsc_log_in_dram_class = NULL;

	fp_delay_start_func(work);

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
	KUNIT_CASE(test_client_test_all),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_client_test",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

