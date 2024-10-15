#include <kunit/test.h>

#include "../logs/scsc_logring_main.h"
#include "../logs/scsc_logring_debugfs.h"

extern int (*fp_debugfile_open)(struct inode *ino, struct file *filp);
extern int (*fp_debugfile_release)(struct inode *ino, struct file *filp);
extern size_t (*fp_init_cached_read)(struct scsc_ibox *i, size_t retrieved_bytes, size_t *count);
extern size_t (*fp_process_cached_read_data)(struct scsc_ibox *i, size_t *count);
extern ssize_t (*fp_samsg_read)(struct file *filp, char __user *ubuf, size_t count, loff_t *f_pos);
extern int (*fp_samsg_open)(struct inode *ino, struct file *filp);
extern int (*fp_samsg_release)(struct inode *ino, struct file *filp);
extern int (*fp_debugfile_open_snapshot)(struct inode *ino, struct file *filp);
extern ssize_t (*fp_samlog_read)(struct file *filp, char __user *ubuf, size_t count, loff_t *f_pos);
extern int (*fp_statfile_open)(struct inode *ino, struct file *filp);
extern int (*fp_statfile_release)(struct inode *ino, struct file *filp);
extern ssize_t (*fp_statfile_read)(struct file *filp, char __user *ubuf, size_t count, loff_t *f_pos);
extern int (*fp_samwritefile_open)(struct inode *ino, struct file *filp);
extern int (*fp_samwritefile_release)(struct inode *ino, struct file *filp);
extern ssize_t (*fp_samwritefile_write)(struct file *filp, const char __user *ubuf, size_t count, loff_t *f_pos);

#ifndef CONFIG_SCSC_LOGRING_DEBUGFS
extern void (*fp_samlog_remove_chrdev)(struct scsc_debugfs_info *di);
extern int (*fp_samlog_remove_char_dev)(struct scsc_debugfs_info *di, struct cdev *cdev);
#endif

extern void (*fp_scsc_ring_buffer_overlap_append)(struct scsc_ring_buffer *rb, const char *srcbuf, int slen, const char *hbuf, int hlen);

static void test_all(struct kunit *test)
{
	struct inode ino;
	struct file filp;
	struct scsc_ibox i;
	struct scsc_ibox *ibox;
	size_t retrieved_bytes;
	size_t count;
	struct scsc_ring_buffer rb;
	char *user_buf;
	char *buf;
	loff_t ppos;
	struct scsc_debugfs_info info;
	struct write_config wconfig;

	scsc_logring_unregister_mx_cb(NULL);

	scsc_logring_unregister_mx_cb(NULL);

	fp_debugfile_open(&ino, &filp);

	fp_debugfile_release(&ino, &filp);

	retrieved_bytes = 10;
	count = 9;

	fp_init_cached_read(&i, retrieved_bytes, &count);

	retrieved_bytes = 9;
	count = 10;

	fp_init_cached_read(&i, retrieved_bytes, &count);

	fp_process_cached_read_data(&i, &count);

	i.t_used = 2;

	fp_process_cached_read_data(&i, &count);

	rb.bsz = 5;
	rb.ssz = 2;
	rb.buf = "ABCDEFGHIJK";
	rb.head = 3;
	rb.spare = &rb.buf[rb.bsz];

	i.rb = &rb;
	buf = "QRS";
	i.tbuf = buf;
	i.t_used = 0;
	i.tsz = 1;
	i.f_pos = 1;

	ibox = kunit_kzalloc(test, sizeof(*ibox), GFP_KERNEL);
	memcpy(ibox, &i, sizeof(*ibox));
	filp.private_data = ibox;

	ppos = 1;
	user_buf = "QWERTY";
	fp_scsc_ring_buffer_overlap_append(&rb, rb.buf+3, 1, buf, 3);

	fp_samsg_read(&filp, user_buf, 1, &ppos);

	debugfile_llseek(&filp, 1, 0);

#ifndef CONFIG_SCSC_LOGRING_DEBUGFS
	info.rb = &rb;
	ino.i_cdev = &info.cdev_samsg;

#endif
	fp_samsg_open(&ino, &filp);

	fp_samsg_release(&ino, &filp);

#ifndef CONFIG_SCSC_LOGRING_DEBUGFS
	ino.i_cdev = &info.cdev_samlog;
#endif
	ino.i_private = &rb;
	filp.private_data = NULL;

	fp_debugfile_open_snapshot(&ino, &filp);

	filp.private_data = &i;
	i.t_used = 0;
	fp_scsc_ring_buffer_overlap_append(&rb, rb.buf+3, 1, buf, 3);

	fp_samlog_read(&filp, user_buf, 1, &ppos);

#ifndef CONFIG_SCSC_LOGRING_DEBUGFS
	ino.i_cdev = &info.cdev_stat;
#endif

	fp_statfile_open(&ino, &filp);

	fp_statfile_release(&ino, &filp);

	filp.private_data = &rb;

	fp_statfile_read(&filp, user_buf, 1, &ppos);

	filp.private_data = NULL;

	fp_samwritefile_open(&ino, &filp);

	fp_samwritefile_release(&ino, &filp);

	wconfig.buf_sz = 10;
	wconfig.fmt = "ABCD";
	filp.private_data = &wconfig;
	user_buf = "qwe";

	fp_samwritefile_write(&filp, user_buf, 5, &ppos);

	samlog_debugfs_exit(NULL);

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
		.name = "test_scsc_logring_debugfs",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

