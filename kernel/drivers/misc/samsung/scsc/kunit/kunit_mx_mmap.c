#include <kunit/test.h>
#ifdef CONFIG_SOC_S5E8825
#include "../platform_mif_s5e8825.h"
#endif
#ifdef CONFIG_SOC_S5E8535
#include "../platform_mif_s5e8535.h"
#endif
#ifdef CONFIG_WLBT_REFACTORY
#include "../platform_mif.h"
#endif
#include <linux/platform_device.h>

#define copy_from_user(args...)			(0)
#define remap_pfn_range(args...)		(0)
#define gdb_transport_send(args...)		((void *)0)
#define wake_up_interruptible(args...)	(0)
#define wait_event_interruptible(args...)	(-1)
#define poll_wait(args...)			((void *)0)
#define kfifo_is_empty(args...)			(1)


struct mx_mmap_dev *test_alloc_mx_dev(struct kunit *test)
{
	struct scsc_mif_abs *scscmif;
	struct mx_mmap_dev *mx_dev;
	struct platform_device pdev;
	struct device dev;
	struct platform_mif *platform;

	pdev.dev = dev;
	set_test_in_platform_mif(test);
	set_property(0x99);
	scscmif = get_mif();

	mx_dev = kunit_kzalloc(test, sizeof(*mx_dev), GFP_KERNEL);
	mx_dev->mif_abs = scscmif;
	mx_dev->gdb_transport = kunit_kzalloc(test, sizeof(struct gdb_transport), GFP_KERNEL);

	platform = container_of(scscmif, struct platform_mif, interface);
	// platform_mif_map(scscmif, 0x00);

	return mx_dev;
}

struct cdev *get_cdev(struct mx_mmap_dev *mmap_dev)
{
	return &mmap_dev->cdev;
}

struct scsc_mif_abs *get_scsc_mif(struct mx_mmap_dev *mmap_dev)
{
	return mmap_dev->mif_abs;
}

static ssize_t mx_gdb_write(struct file *filp, const char __user *ubuf, size_t len, loff_t *offset);
ssize_t (*fp_mx_gdb_write)(struct file *filp, const char __user *ubuf, size_t len, loff_t *offset) = &mx_gdb_write;

static ssize_t mx_gdb_read(struct file *filp, char __user *buf, size_t len, loff_t *offset);
ssize_t (*fp_mx_gdb_read)(struct file *filp, char __user *buf, size_t len, loff_t *offset) = &mx_gdb_read;

static unsigned int mx_gdb_poll(struct file *filp, poll_table *wait);
unsigned int (*fp_mx_gdb_poll)(struct file *filp, poll_table *wait) = &mx_gdb_poll;

static void __exit mx_mmap_cleanup(void);
void (*fp_mx_mmap_cleanup)(void) = &mx_mmap_cleanup;
