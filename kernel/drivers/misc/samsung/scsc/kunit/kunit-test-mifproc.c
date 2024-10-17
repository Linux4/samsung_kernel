#include <kunit/test.h>
#include <linux/fs.h>
#include "../mxman.h"
#include "../miframman.h"

extern int (*fp_mifprocfs_open_file_generic)(struct inode *inode, struct file *file);
extern ssize_t (*fp_mifprocfs_ramman_total_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mifprocfs_ramman_offset_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mifprocfs_ramman_start_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mifprocfs_ramman_size_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mifprocfs_ramman_free_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern ssize_t (*fp_mifprocfs_ramman_used_read)(struct file *file, char __user *user_buf, size_t count, loff_t *ppos);
extern int (*fp_mifprocfs_ramman_list_show)(struct seq_file *m, void *v);

static void test_all(struct kunit *test)
{
	struct file file;
	char user_buf;
	loff_t ppos = 1;
	struct miframman *ramman;
	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct seq_file m;

	mx = test_alloc_mxman(test);
	mx->start_dram = ((void *)0);
	mx->size_dram = (size_t)(0);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;

	ramman = scsc_mx_get_ramman2(mx->mx);

	//fp_mifprocfs_open_file_generic(&inode, &file);
	file.private_data = ramman;

	fp_mifprocfs_ramman_total_read(&file, &user_buf, 1, &ppos);

	fp_mifprocfs_ramman_offset_read(&file, &user_buf, 1, &ppos);

	fp_mifprocfs_ramman_start_read(&file, &user_buf, 1, &ppos);

	fp_mifprocfs_ramman_size_read(&file, &user_buf, 1, &ppos);

	fp_mifprocfs_ramman_free_read(&file, &user_buf, 1, &ppos);

	fp_mifprocfs_ramman_used_read(&file, &user_buf, 1, &ppos);

	m.private = ramman;

	fp_mifprocfs_ramman_list_show(&m, NULL);

	mifproc_remove_proc_dir();

	//mifproc_create_ramman_proc_dir(ramman);

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
		.name = "test_mifproc",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

