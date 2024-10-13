#include <kunit/test.h>
#include <linux/fs.h>
#include "../mxman.h"
#include "../mxproc.h"
#include "../srvman.h"

extern ssize_t (*fp_mx_procfs_mx_fail_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_fail_write)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_freeze_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_freeze_write)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_panic_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_panic_write)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_lastpanic_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_suspend_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_suspend_count_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_recovery_count_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_boot_count_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_suspend_write)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_status_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_services_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_wlbt_stat_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_rf_hw_ver_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_rf_hw_name_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_release_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mx_procfs_mx_ttid_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);

static void test_all(struct kunit *test)
{
	struct file file;
	char user_buf[256];
	loff_t ppos = 1;

	struct mxproc *mxproc;
	struct miframman *ramman;
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct srvman srvman;


	mx = test_alloc_mxman(test);
	mx->start_dram = ((void *)0);
	mx->size_dram = (size_t)(0);

	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;

	mxproc = &mx->mxproc;
	mxproc->mxman = mx;

	srvman.error = false;
	srvman_init(&srvman, scscmx);
	set_srvman(scscmx, srvman);

	file.private_data = mxproc;

	fp_mx_procfs_mx_fail_read(&file, &user_buf, 1, &ppos);

	user_buf[0] = '1';
	fp_mx_procfs_mx_fail_write(&file, &user_buf, 2, &ppos);

	user_buf[0] = '2';
	fp_mx_procfs_mx_fail_write(&file, &user_buf, 2, &ppos);

	user_buf[0] = '3';
	fp_mx_procfs_mx_fail_write(&file, &user_buf, 2, &ppos);

	user_buf[0] = '4';
	fp_mx_procfs_mx_fail_write(&file, &user_buf, 2, &ppos);

	user_buf[0] = '0';
	fp_mx_procfs_mx_fail_write(&file, &user_buf, 2, &ppos);

	fp_mx_procfs_mx_freeze_read(&file, &user_buf, 1, &ppos);

	fp_mx_procfs_mx_freeze_write(&file, &user_buf, 1, &ppos);

	fp_mx_procfs_mx_panic_read(&file, &user_buf, 1, &ppos);

	user_buf[0] = '0';
	fp_mx_procfs_mx_panic_write(&file, &user_buf, 2, &ppos);

	user_buf[0] = '1';
	fp_mx_procfs_mx_panic_write(&file, &user_buf, 2, &ppos);

	user_buf[0] = '2';
	fp_mx_procfs_mx_panic_write(&file, &user_buf, 2, &ppos);

	user_buf[0] = '3';
	fp_mx_procfs_mx_panic_write(&file, &user_buf, 2, &ppos);

	fp_mx_procfs_mx_lastpanic_read(&file, &user_buf, 1, &ppos);

	fp_mx_procfs_mx_suspend_read(&file, &user_buf, 1, &ppos);

	fp_mx_procfs_mx_suspend_count_read(&file, &user_buf, 1, &ppos);

	fp_mx_procfs_mx_recovery_count_read(&file, &user_buf, 1, &ppos);

	fp_mx_procfs_mx_boot_count_read(&file, &user_buf, 1, &ppos);

	fp_mx_procfs_mx_suspend_write(&file, &user_buf, 1, &ppos);

	fp_mx_procfs_mx_status_read(&file, &user_buf, 1, &ppos);

	mx->mxman_state = MXMAN_STATE_STARTING;
	fp_mx_procfs_mx_status_read(&file, &user_buf, 1, &ppos);

	mx->mxman_state = MXMAN_STATE_STARTED_WLAN;
	fp_mx_procfs_mx_status_read(&file, &user_buf, 1, &ppos);

	mx->mxman_state = MXMAN_STATE_FAILED_WLAN;
	fp_mx_procfs_mx_status_read(&file, &user_buf, 1, &ppos);

	mx->mxman_state = MXMAN_STATE_FROZEN;
	fp_mx_procfs_mx_status_read(&file, &user_buf, 1, &ppos);

	mx->mxman_state = MXMAN_STATE_STARTED_WLAN;
	fp_mx_procfs_mx_status_read(&file, &user_buf, 1, &ppos);

	mx->mxman_state = MXMAN_STATE_STARTED_WPAN;
	fp_mx_procfs_mx_status_read(&file, &user_buf, 1, &ppos);

	mx->mxman_state = MXMAN_STATE_STARTED_WLAN_WPAN;
	fp_mx_procfs_mx_status_read(&file, &user_buf, 1, &ppos);

	mx->mxman_state = MXMAN_STATE_FAILED_WPAN;
	fp_mx_procfs_mx_status_read(&file, &user_buf, 1, &ppos);

	mx->mxman_state = MXMAN_STATE_FAILED_WLAN_WPAN;
	fp_mx_procfs_mx_status_read(&file, &user_buf, 1, &ppos);

	//fp_mx_procfs_mx_services_read(&file, &user_buf, 1, &ppos);

	fp_mx_procfs_mx_wlbt_stat_read(&file, &user_buf, 1, &ppos);

	mxproc_create_ctrl_proc_dir(mxproc, mx);
	mxproc_remove_ctrl_proc_dir(mxproc);

	fp_mx_procfs_mx_rf_hw_ver_read(&file, &user_buf, 1, &ppos);

	fp_mx_procfs_mx_rf_hw_name_read(&file, &user_buf, 1, &ppos);

	fp_mx_procfs_mx_release_read(&file, &user_buf, 1, &ppos);

	fp_mx_procfs_mx_ttid_read(&file, &user_buf, 1, &ppos);

	mxproc_create_info_proc_dir(mxproc, mx);
	mxproc_remove_info_proc_dir(mxproc);

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
		.name = "test_mxproc",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

