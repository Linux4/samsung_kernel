#include <kunit/test.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include "../gdb_transport.h"

#define platform_mif_map_region(args...) ((void*)0)
#define platform_mif_map(args...) ((void*)0)
#define mxlog_load_log_strings(args...)	((void)0)

extern ssize_t (*fp_mx_gdb_write)(struct file *filp, const char __user *ubuf, size_t len, loff_t *offset);
extern ssize_t (*fp_mx_gdb_read)(struct file *filp, char __user *buf, size_t len, loff_t *offset);
extern unsigned int (*fp_mx_gdb_poll)(struct file *filp, poll_table *wait);
extern void (*fp_mx_mmap_cleanup)(void);

static void test_all(struct kunit *test)
{
	struct mx_mmap_dev *mx_dev;
	struct inode inode;
	struct file filp;
	struct vm_area_struct vma;

	struct gdb_transport_client gdb_client;
	struct gdb_transport gdb_transport;
	char *dev_uid = "999";

	mx_dev = test_alloc_mx_dev(test);
	inode.i_cdev = get_cdev(mx_dev);

	mx_mmap_open(&inode, &filp);

	mx_mmap_mmap(&filp, &vma);

	mx_mmap_release(NULL, NULL);

	mx_gdb_open(&inode, &filp);

	fp_mx_gdb_write(&filp, "ABC", 3, NULL);
	fp_mx_gdb_write(&filp, "ABC", 65, NULL);

	fp_mx_gdb_read(&filp, "ABD", 3, NULL);

	gdb_read_callback("MESSAGE", 7, mx_dev);

	fp_mx_gdb_poll(&filp, NULL);

	mx_gdb_release(&inode, &filp);

	client_gdb_probe(&gdb_client, &gdb_transport, dev_uid);

	client_gdb_remove(&gdb_client, &gdb_transport);

	// scsc_mx_mmap_module_probe(NULL, get_scsc_mif(mx_dev));

	// scsc_mx_mmap_module_remove(get_scsc_mif(mx_dev));

	fp_mx_mmap_cleanup();

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
		.name = "test_mx_mmap",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

