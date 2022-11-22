#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/kmsg_dump.h>
#include <linux/reboot.h>
#include <linux/uaccess.h>
#include <mach/system.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

#include "srtpm.h"

static void srtpm_poweroff()
{
	printk(KERN_ALERT "Power down.\n");
	if (pm_power_off)pm_power_off();
}

static ssize_t __srtpm_get_val(char *buf, int len)
{
	int val = 0;

	return snprintf(buf, len, "%d\n", val);
}


static ssize_t __srtpm_set_val(const char *buf, size_t count)
{
	int val = 0;
	val = kstrtol(buf, NULL, 10);
	if (val == 1)srtpm_poweroff();
	return count;
}


static ssize_t srtpm_proc_read(char *page, char **start, off_t off, int count,
			       int *eof, void *data)
{
	if (off > 0) {
		*eof = 1;
		return 0;
	}

	return __srtpm_get_val(page, count);
}

static ssize_t srtpm_proc_write(struct file *filp, const char __user * buff,
				unsigned long len, void *data)
{
	int err = 0;
	char *page = NULL;

	if (len > PAGE_SIZE) {
		printk(KERN_ALERT "The buff is too large:%lu.\n", len);
		return -EFAULT;
	}

	page = (char *)kmalloc(len, GFP_KERNEL);
	if (!page) {
		printk(KERN_ALERT "Failed to alloc page.\n");
		return -ENOMEM;
	}
	if (copy_from_user(page, buff, len)) {
		printk(KERN_ALERT "Failed to copy buff from user.\n");
		err = -EFAULT;
		goto out;
	}

	err = __srtpm_set_val(page, len);

out:
	kfree(page);
	return err;
}
static const struct file_operations proc_srtpm_operations = {
	.owner		= THIS_MODULE,
	.write		= srtpm_proc_write,
	.read		= srtpm_proc_read,
};

static void srtpm_create_proc(void)
{
	struct proc_dir_entry *entry;
	entry = proc_create(SRTPM_DEVICE_PROC_NAME, 0, NULL, &proc_srtpm_operations);
}


static void srtpm_remove_proc(void)
{
	remove_proc_entry(SRTPM_DEVICE_PROC_NAME, NULL);
}


static int __init srtpm_init(void)
{

	srtpm_create_proc();

	printk(KERN_ALERT "Successed to initialize srtpm device.\n");
	return 0;
}


static void __exit srtpm_exit(void)
{
	printk(KERN_ALERT "Destroy srtpm device.\n");

	srtpm_remove_proc();
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("First Android Device");

module_init(srtpm_init);
module_exit(srtpm_exit);
