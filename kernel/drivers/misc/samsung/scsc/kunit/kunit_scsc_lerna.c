#include <kunit/test.h>

#define wait_for_completion_timeout(args...)	(1)
#define copy_to_user(args...)					(0)
static const void *scsc_lerna_pending;
void set_header(struct scsc_lerna_cmd_header *header)
{
	scsc_lerna_pending = header;
}

struct scsc_lerna_cmd_header *test_alloc_header(struct kunit *test)
{
	struct scsc_lerna_cmd_header *header;

	header = kunit_kzalloc(test, sizeof(*header), GFP_KERNEL);
	return header;
}

static struct file_operations scsc_lerna_fops;
struct file_operations *get_lerna_fops(void)
{
	return &scsc_lerna_fops;
}

static int scsc_lerna_chardev_open(struct inode *inodep, struct file *filep);
int (*fp_scsc_lerna_chardev_open)(struct inode *inodep, struct file *filep) = &scsc_lerna_chardev_open;

static ssize_t scsc_lerna_chardev_read(struct file *filep, char *buffer, size_t len, loff_t *offset);
ssize_t (*fp_scsc_lerna_chardev_read)(struct file *filep, char *buffer, size_t len, loff_t *offset) = &scsc_lerna_chardev_read;

static ssize_t scsc_lerna_chardev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset);
ssize_t (*fp_scsc_lerna_chardev_write)(struct file *filep, const char *buffer, size_t len, loff_t *offset) = &scsc_lerna_chardev_write;

static int scsc_lerna_chardev_release(struct inode *inodep, struct file *filep);
int (*fp_scsc_lerna_chardev_release)(struct inode *inodep, struct file *filep) = &scsc_lerna_chardev_release;