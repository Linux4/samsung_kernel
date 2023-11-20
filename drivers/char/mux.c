/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <linux/sprdmux.h>

struct mux_device {
	struct mux_init_data	*init;
	int			major;
	int			minor;
	struct cdev		cdev;
};

struct mux_channel {
	int		mux_id;
	int		line;
};

static struct class		*mux_class;

static int mux_open(struct inode *inode, struct file *filp)
{
	int minor = iminor(filp->f_path.dentry->d_inode);
	struct mux_device *mux;
	struct mux_channel *channel;
	int rval, mux_id, line;

	mux = container_of(inode->i_cdev, struct mux_device, cdev);

	if (NULL == mux) {
		printk(KERN_ERR "MUX: Error %s mux_dev is NULL\n", __FUNCTION__);
		return -ENODEV;
	}

	mux_id = mux->init->id;

	rval = ts0710_mux_status(mux_id);
	if (rval != 0) {
		printk(KERN_ERR "MUX: Error %s [%d] mux_status is Not OK\n", __FUNCTION__, mux->init->id);
		filp->private_data = NULL;
		return -ENODEV;
	}

	printk(KERN_ERR "MUX: %s mux_id  = %d, minor = %d, mux->minor = %d\n", __FUNCTION__, mux_id , minor, mux->minor);

	line = minor - mux->minor;

	rval = ts0710_mux_open(mux_id, line);
	if (rval != 0) {
		printk(KERN_ERR "MUX: Error %s id[%d] line[%d] mux_open is Not OK\n", __FUNCTION__, mux->init->id, line);
		filp->private_data = NULL;
		return rval;
	}

	channel = kmalloc(sizeof(struct mux_channel), GFP_KERNEL);
	if (!channel) {
		return -ENOMEM;
	}

	channel->mux_id = mux_id;
	channel->line = line;

	filp->private_data = channel;

	return 0;
}

static int mux_release(struct inode *inode, struct file *filp)
{
	struct mux_channel *channel = filp->private_data;

	if (channel == NULL) {
		printk(KERN_ERR "MUX: Error %s channel is NULL\n", __FUNCTION__);
		return -ENODEV;
	}

	ts0710_mux_close(channel->mux_id, channel->line);

	kfree(channel);

	return 0;
}

static ssize_t mux_read(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos)
{
	int timeout = -1;
	struct mux_channel *channel = filp->private_data;

	if (channel == NULL) {
		printk(KERN_ERR "MUX: Error %s channel is NULL\n", __FUNCTION__);
		return -ENODEV;
	}

	if (filp->f_flags & O_NONBLOCK) {
		timeout = 0;
	}

	return ts0710_mux_read(channel->mux_id, channel->line, buf, count, timeout);

}

static ssize_t mux_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	int timeout = -1;
	struct mux_channel *channel = filp->private_data;

	if (channel == NULL) {
		printk(KERN_ERR "MUX: Error %s channel is NULL\n", __FUNCTION__);
		return -ENODEV;
	}

	if (filp->f_flags & O_NONBLOCK) {
		timeout = 0;
	}

	return ts0710_mux_write(channel->mux_id, channel->line, buf, count, timeout);

}

static unsigned int mux_poll(struct file *filp, poll_table *wait)
{
	struct mux_channel *channel = filp->private_data;

	if (channel == NULL) {
		printk(KERN_ERR "MUX: Error %s channel is NULL\n", __FUNCTION__);
		return -ENODEV;
	}

	return ts0710_mux_poll_wait(channel->mux_id, channel->line, filp, wait);
}

static long mux_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct mux_channel *channel = filp->private_data;

	if (channel == NULL) {
		printk(KERN_ERR "MUX: Error %s channel is NULL\n", __FUNCTION__);
		return -ENODEV;
	}

	return ts0710_mux_mux_ioctl(channel->mux_id, channel->line, cmd, arg);
}

static const struct file_operations mux_fops = {
	.open		= mux_open,
	.release	= mux_release,
	.read		= mux_read,
	.write		= mux_write,
	.poll		= mux_poll,
	.unlocked_ioctl	= mux_ioctl,
	.owner		= THIS_MODULE,
	.llseek		= default_llseek,
};

static int  mux_probe(struct platform_device *pdev)
{
	dev_t devid;
	int i, rval;
	struct mux_device *mux;
	struct mux_init_data *init = pdev->dev.platform_data;

	mux = kzalloc(sizeof(struct mux_device), GFP_KERNEL);
	if (mux == NULL) {
		printk(KERN_ERR "MUX: Error %s no memory for mux\n", __FUNCTION__);
		return -ENOMEM;
	}

	rval = alloc_chrdev_region(&devid, 0, init->num, init->name);
	if (rval != 0) {
		kfree(mux);
		printk(KERN_ERR "MUX: Error %s no memory for dev_region\n", __FUNCTION__);
		return rval;
	}

	cdev_init(&(mux->cdev), &mux_fops);

	rval = cdev_add(&(mux->cdev), devid, init->num);
	if (rval != 0) {
		kfree(mux);
		unregister_chrdev_region(devid, init->num);
		printk(KERN_ERR "MUX: Error %s add error\n", __FUNCTION__);
		return rval;
	}

	mux->major = MAJOR(devid);
	mux->minor = MINOR(devid);

	if (init->num > 1) {
		for (i = 0; i < init->num; i++) {
			device_create(mux_class, NULL,
				MKDEV(mux->major, mux->minor + i),
				NULL, "%s%d", init->name, i);
		}
	} else {
		device_create(mux_class, NULL,
			MKDEV(mux->major, mux->minor),
			NULL, "%s", init->name);
	}

	mux->init = init;

	platform_set_drvdata(pdev, mux);

	return 0;
}

static int  mux_remove(struct platform_device *pdev)
{
	int i;
	struct mux_device *mux = platform_get_drvdata(pdev);

	for (i = 0; i < mux->init->num; i++) {
		device_destroy(mux_class, MKDEV(mux->major, mux->minor + i));
	}

	cdev_del(&(mux->cdev));

	unregister_chrdev_region(MKDEV(mux->major, mux->minor), mux->init->num);

	kfree(mux);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver mux_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "mux",
	},
	.probe = mux_probe,
	.remove = mux_remove,
};

static int __init mux_init(void)
{
	printk(KERN_ERR "MUX: %s entered\n", __FUNCTION__);

	ts0710_mux_init();

	mux_class = class_create(THIS_MODULE, "mux");
	if (IS_ERR(mux_class))
		return PTR_ERR(mux_class);

	return platform_driver_register(&mux_driver);
}

static void __exit mux_exit(void)
{
	printk(KERN_ERR "MUX: %s entered\n", __FUNCTION__);
	ts0710_mux_exit();
	class_destroy(mux_class);
	platform_driver_unregister(&mux_driver);
}

module_init(mux_init);
module_exit(mux_exit);

MODULE_AUTHOR("Wu Jiaoyou");
MODULE_DESCRIPTION("mux driver");
MODULE_LICENSE("GPL");
