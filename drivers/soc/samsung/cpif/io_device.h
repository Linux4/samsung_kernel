/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __IO_DEVICE_H__
#define __IO_DEVICE_H__

int iodev_open(struct inode *inode, struct file *filp);
int iodev_release(struct inode *inode, struct file *filp);
unsigned int iodev_poll(struct file *filp, struct poll_table_struct *wait);
long iodev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
ssize_t iodev_write(struct file *filp, const char __user *data,
		    size_t count, loff_t *fpos);
ssize_t iodev_read(struct file *filp, char *buf, size_t count, loff_t *fpos);

const struct file_operations *get_iodev_io_fops(void);
static inline const struct file_operations *get_ipc_io_fops(void)
{
	return get_iodev_io_fops();
}

const struct file_operations *get_bootdump_io_fops(void);

#endif

