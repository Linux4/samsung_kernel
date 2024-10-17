// SPDX-License-Identifier: GPL-2.0
/*
 * (C) 2021 Carles Pey <cpey@pm.me>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <kcfi-helper.h>

MODULE_LICENSE("GPL");

#define DEVICE_NAME "kcfi_test"
#define BUF_LEN 20

typedef struct func_table {
	void (*fn1)(void);
	void (*fn2)(char);
	void (*fn3)(char);
	char (*fn4)(char);
} func_table;

struct dentry *file;
func_table ft;

static int device_open(struct inode *inode, struct file *filp)
{
	filp->f_pos = 0;
	ft.fn1 = func_void;
	ft.fn2 = func_char;
	ft.fn3 = func_char_bis;
	ft.fn4 = func_char_ret_char;

	pr_info("fn1: %p\n", ft.fn1);
	pr_info("fn2: %p\n", ft.fn2);
	pr_info("fn3: %p\n", ft.fn3);
	pr_info("fn4: %p\n", ft.fn4);

	return 0;
}

static ssize_t device_write(struct file *filp, const char *buf,
	size_t count, loff_t *position)
{
	if (copy_from_user((char *) &ft + filp->f_pos, buf, count) != 0)
		return -EFAULT;

	return count;
}

static ssize_t device_read(struct file *filp, char *buf,
	size_t count, loff_t *position)
{
	if (copy_to_user(buf, (char *) &ft + filp->f_pos, count) != 0)
		return -EFAULT;

	return count;
}

static long device_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	switch (cmd) {
	case CALL_FN1:
		ft.fn1();
		break;
	case CALL_FN2:
		ft.fn2(2);
		break;
	case CALL_FN3:
		ft.fn3(3);
		break;
	case CALL_FN4:
		ft.fn4(4);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static loff_t device_seek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos = 0;
	switch(whence) {
	case 0: /* SEEK_SET */
		newpos = off;
		break;
	case 1: /* SEEK_CUR */
		newpos += filp->f_pos + off;
		break;
	case 2: /* SEEK_END */
		newpos = sizeof(func_table) + off;
		break;
	default:
		return -EINVAL;
	}
	if (newpos < 0) return -EINVAL;
	filp->f_pos = newpos;
	return newpos;
}

static int device_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations my_fops = {
	.owner = THIS_MODULE,
	.open = device_open,
	.write = device_write,
	.read = device_read,
	.unlocked_ioctl = device_ioctl,
	.llseek = device_seek,
	.release = device_release
};

static int __init test_kcfi_init(void)
{
	pr_info("kCFI Test");
	file = debugfs_create_file(DEVICE_NAME, 0200, NULL, NULL, &my_fops);

	return 0;
}

static void __exit test_kcfi_exit(void)
{
	pr_info("Unloaded kCFI Test");
}

module_init(test_kcfi_init);
module_exit(test_kcfi_exit);
