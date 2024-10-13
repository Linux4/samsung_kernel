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

#include <linux/spool.h>
#include <linux/sipc.h>

struct spool_device {
	struct spool_init_data	*init;
	int			major;
	int			minor;
	struct cdev		cdev;
};

struct spool_sblock {
	uint8_t			dst;
	uint8_t			channel;
};

static struct class		*spool_class;

static int spool_open(struct inode *inode, struct file *filp)
{
	static struct spool_device *spool;
	struct spool_sblock *sblock;

	spool = container_of(inode->i_cdev, struct spool_device, cdev);
	sblock = kmalloc(sizeof(struct spool_sblock), GFP_KERNEL);
	if (!sblock) {
		return -ENOMEM;
	}
	filp->private_data = sblock;

	sblock->dst = spool->init->dst;
	sblock->channel = spool->init->channel;

	return 0;
}

static int spool_release(struct inode *inode, struct file *filp)
{
	struct spool_sblock *sblock = filp->private_data;

	kfree(sblock);

	return 0;
}

static ssize_t spool_read(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos)
{
	struct spool_sblock *sblock = filp->private_data;
	int timeout = -1;
	int ret = 0;
	int rdsize = 0;
	struct sblock blk = {0};

	if (filp->f_flags & O_NONBLOCK) {
		timeout = 0;
	}

	if((ret = sblock_receive(sblock->dst, sblock->channel, &blk, timeout)) < 0){
		pr_debug("spool_read: failed to receive block!\n");
		return ret;
	}

	rdsize = blk.length > count ? count : blk.length;

	if(copy_to_user(buf, blk.addr, rdsize)){
		pr_debug("spool_read: failed to copy to user!\n");
		ret = -EFAULT;
	}else{
		ret = rdsize;
	}

	if(sblock_release(sblock->dst, sblock->channel, &blk)){
		pr_debug("failed to release block!\n");
	}

	return ret;
}

static ssize_t spool_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct spool_sblock *sblock = filp->private_data;
	int timeout = -1;
	int ret = 0;
	int wrsize = 0;
	int pos = 0;
	struct sblock blk = {0};
	size_t len = count;

	if(filp->f_flags & O_NONBLOCK){
		timeout = 0;
	}

	do{
		if((ret = sblock_get(sblock->dst, sblock->channel, &blk, timeout)) < 0){
		printk(KERN_WARNING "spool_write: failed to get block!\n");
		return ret;
		}

		wrsize = (blk.length > len ? len : blk.length);
		pr_debug("spool_write: blk_len %d, count %d, wsize %d\n", blk.length, len, wrsize);
		if(copy_from_user(blk.addr, buf + pos, wrsize)){
			printk(KERN_WARNING "spool_write: failed to copy from user!\n");
			ret = -EFAULT;
		}else{
			blk.length = wrsize;
			len -= wrsize;
			pos += wrsize;
		}

		if(sblock_send(sblock->dst, sblock->channel, &blk)){
			pr_debug("spool_write: failed to send block!");
		}

		pr_debug("spool_write len= %u, ret= %d\n", len, ret);
	}while(len > 0 && ret == 0);

	return count - len;
}

static unsigned int spool_poll(struct file *filp, poll_table *wait)
{
	return 0;
}

static long spool_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static const struct file_operations spool_fops = {
	.open		= spool_open,
	.release	= spool_release,
	.read		= spool_read,
	.write		= spool_write,
	.poll		= spool_poll,
	.unlocked_ioctl	= spool_ioctl,
	.owner		= THIS_MODULE,
	.llseek		= default_llseek,
};

static int __devinit spool_probe(struct platform_device *pdev)
{
	struct spool_init_data *init = pdev->dev.platform_data;
	struct spool_device *spool;
	dev_t devid;
	int rval;

	rval = sblock_create(init->dst, init->channel, init->txblocknum,
		init->txblocksize, init->rxblocknum, init->rxblocksize);
	if (rval != 0) {
		printk(KERN_ERR "Failed to create sblock: %d\n", rval);
		return rval;
	}

	spool = kzalloc(sizeof(struct spool_device), GFP_KERNEL);
	if (spool == NULL) {
		sblock_destroy(init->dst, init->channel);
		printk(KERN_ERR "Failed to allocate spool_device\n");
		return -ENOMEM;
	}

	rval = alloc_chrdev_region(&devid, 0, 1, init->name);
	if (rval != 0) {
		sblock_destroy(init->dst, init->channel);
		kfree(spool);
		printk(KERN_ERR "Failed to alloc spool chrdev\n");
		return rval;
	}
	cdev_init(&(spool->cdev), &spool_fops);
	rval = cdev_add(&(spool->cdev), devid, 1);
	if (rval != 0) {
		sblock_destroy(init->dst, init->channel);
		kfree(spool);
		unregister_chrdev_region(devid, 1);
		printk(KERN_ERR "Failed to add spool cdev\n");
		return rval;
	}

	spool->major = MAJOR(devid);
	spool->minor = MINOR(devid);

        device_create(spool_class, NULL,
        MKDEV(spool->major, spool->minor),
        NULL, "%s", init->name);

	spool->init = init;

	platform_set_drvdata(pdev, spool);

	return 0;
}

static int __devexit spool_remove(struct platform_device *pdev)
{
	struct spool_device *spool = platform_get_drvdata(pdev);
	device_destroy(spool_class, MKDEV(spool->major, spool->minor));

	cdev_del(&(spool->cdev));
	unregister_chrdev_region(
	MKDEV(spool->major, spool->minor), 1);

	sblock_destroy(spool->init->dst, spool->init->channel);
	kfree(spool);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver spool_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "spool",
	},
	.probe = spool_probe,
	.remove = __devexit_p(spool_remove),
};

static int __init spool_init(void)
{
	spool_class = class_create(THIS_MODULE, "spool");
	if (IS_ERR(spool_class))
		return PTR_ERR(spool_class);

	return platform_driver_register(&spool_driver);
}

static void __exit spool_exit(void)
{
	class_destroy(spool_class);
	platform_driver_unregister(&spool_driver);
}

module_init(spool_init);
module_exit(spool_exit);

MODULE_AUTHOR("Qiu Yi");
MODULE_DESCRIPTION("SIPC/SPOOL driver");
MODULE_LICENSE("GPL");
