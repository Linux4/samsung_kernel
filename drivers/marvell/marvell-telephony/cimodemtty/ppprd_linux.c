/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/aio.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/skbuff.h>
#include "common_datastub.h"
#include "data_channel_kernel.h"
#include "psd_data_channel.h"
#include "ppprd_linux.h"

#define INVALID_CID 0xff
#define READQ_MAX_LEN 32

struct ppprd_dev {
	struct psd_user psd_user;
	struct device *dev;
	spinlock_t read_spin;
	struct sk_buff_head rx_q;
	wait_queue_head_t readq;
	int have_data;
};

struct route_header {
	unsigned int length;
};

unsigned char modem_current_cid = INVALID_CID;

unsigned int ppprd_open_count;
DEFINE_SPINLOCK(ppprd_init_lock);
struct ppprd_dev *ppprd_dev;

static int ppprd_init(void);
static void ppprd_exit(void);
static int ppprd_probe(struct platform_device *dev);
static int ppprd_remove(struct platform_device *dev);
static int ppprd_open(struct inode *inode, struct file *filp);
static int ppprd_release(struct inode *inode, struct file *filp);
static void ppprd_dev_release(struct device *dev);
static ssize_t ppprd_read(struct file *filp, char __user *buf,
			  size_t count, loff_t *f_ops);
static ssize_t ppprd_write(struct file *filp, const char __user *buf,
			   size_t count, loff_t *f_ops);
static unsigned int ppprd_poll(struct file *filp, poll_table *wait);
static long ppprd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

static const char * const ppprd_name = "ppprd";

static const struct file_operations ppprd_fops = {
	.open		= ppprd_open,
	.read		= ppprd_read,
	.write		= ppprd_write,
	.release	= ppprd_release,
	.unlocked_ioctl	= ppprd_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= ppprd_ioctl,
#endif
	.poll		= ppprd_poll,
	.owner		= THIS_MODULE,
};

static struct platform_device ppprd_device = {
	.name	= "ppprd",
	.id	= 0,
	.dev	= {
	   .release	= ppprd_dev_release,
	},
};

static struct platform_driver ppprd_driver = {
	.probe	= ppprd_probe,
	.remove	= ppprd_remove,
	.driver	= {
	   .name	= "ppprd",
	   .owner	= THIS_MODULE,
	},
};

static struct miscdevice ppprd_miscdev = {
	MISC_DYNAMIC_MINOR,
	"ppprd",
	&ppprd_fops,
};

/*  define debug macro */
#ifdef PPPRD_DEBUG
#define DBGMSG(fmt, args ...)	pr_debug("PPPRD: " fmt, ## args)
#define ERRMSG(fmt, args ...)	pr_err("PPPRD: " fmt, ## args)
#define ENTER()			pr_debug("PPPRD: ENTER %s\n", __func__)
#define LEAVE()			pr_debug("PPPRD: LEAVE %s\n", __func__)
#define DPRINT(fmt, args ...)	pr_info("PPPRD: " fmt, ## args)
#else
#define DBGMSG(fmt, args ...)	do {} while (0)
#define ERRMSG(fmt, args ...)	pr_err("PPPRD: " fmt, ## args)
#define ENTER()			do {} while (0)
#define LEAVE()			do {} while (0)
#define DPRINT(fmt, args ...)	pr_info("PPPRD: " fmt, ## args)
#endif

int ppprd_relay_message_from_comm(void *userobj, const unsigned char *buf,
				  unsigned int count)
{
	struct route_header *hdr;
	struct sk_buff *skb;
	unsigned long flags;

	ENTER();

	/* means this packet with a wrong cid or we are in CONTROL MODE */
	if (modem_current_cid == INVALID_CID) {
		DBGMSG("modem_current_cid is %02x\n", modem_current_cid);
		return 0; /* the packet is not for PPP */
	} else if (skb_queue_len(&ppprd_dev->rx_q) > READQ_MAX_LEN) {
		DBGMSG("rx_q is too long, drop this message\n");
		return 1;
	} else { /* means this packet should transfer to PPP router,
		* alloc memory, copy data, then add to work list */
		skb = alloc_skb(count+sizeof(struct route_header), GFP_ATOMIC);
		if (skb) {
			skb_reserve(skb, sizeof(struct route_header));
			hdr = (struct route_header *)
				skb_push(skb, sizeof(struct route_header));
			hdr->length = count;
			memcpy(skb_put(skb, count), buf, count);
			skb_queue_tail(&ppprd_dev->rx_q, skb);
			/* acknowledge read & select */
			spin_lock_irqsave(&ppprd_dev->read_spin, flags);
			/* increase the queued message number */
			ppprd_dev->have_data++;
			spin_unlock_irqrestore(&ppprd_dev->read_spin, flags);
			wake_up_interruptible(&ppprd_dev->readq);
		} else {
			ERRMSG("%s: out of memory.\n", __func__);
			return -ENOMEM;
		}
	}
	LEAVE();
	return 1; /*the packet is for PPP */
}

static int __init ppprd_init(void)
{
	int ret;

	ENTER();

	ret = platform_device_register(&ppprd_device);
	if (!ret) {
		ret = platform_driver_register(&ppprd_driver);
		if (ret) {
			ERRMSG("Can not register PPPRD platform driver\n");
			platform_device_unregister(&ppprd_device);
		}
	} else
		ERRMSG("Can not register PPPRD platform device\n");

	LEAVE();
	return ret;
}

static void __exit ppprd_exit(void)
{
	ENTER();

	platform_driver_unregister(&ppprd_driver);
	platform_device_unregister(&ppprd_device);

	LEAVE();
}

static int ppprd_probe(struct platform_device *dev)
{
	int ret;

	ENTER();

	ppprd_dev = kzalloc(sizeof(struct ppprd_dev), GFP_KERNEL);
	if (!ppprd_dev) {
		ERRMSG("%s: unable to allocate memory\n", __func__);
		return -ENOMEM;
	}

	init_waitqueue_head(&ppprd_dev->readq);
	spin_lock_init(&ppprd_dev->read_spin);
	skb_queue_head_init(&ppprd_dev->rx_q);
	ppprd_dev->have_data = 0;
	ppprd_dev->dev = (struct device *)dev;
	ppprd_dev->psd_user.priv = ppprd_dev;
	ppprd_dev->psd_user.on_receive = ppprd_relay_message_from_comm;
	ppprd_dev->psd_user.on_throttle = NULL;
	ret = misc_register(&ppprd_miscdev);
	if (ret) {
		kfree(ppprd_dev);
		ERRMSG("%s: failed to register misc device\n", __func__);
	}

	LEAVE();
	return ret;
}

static int ppprd_remove(struct platform_device *dev)
{
	ENTER();

	misc_deregister(&ppprd_miscdev);
	kfree(ppprd_dev);

	LEAVE();
	return 0;
}

static int ppprd_open(struct inode *inode, struct file *filp)
{
	ENTER();

	if (ppprd_dev)
		filp->private_data = ppprd_dev;
	else
		return -ERESTARTSYS;

	spin_lock(&ppprd_init_lock);

	if (ppprd_open_count) {
		ppprd_open_count++;
		spin_unlock(&ppprd_init_lock);
		DPRINT("ppprd is opened by process id: %d(%s)\n",
			current->tgid, current->comm);
		return 0;
	}

	ppprd_open_count = 1;

	spin_unlock(&ppprd_init_lock);

	LEAVE();
	DBGMSG("ppprd is opened by process id: %d(%s)\n",
		current->tgid, current->comm);
	return 0;
}

static int ppprd_release(struct inode *inode, struct file *filp)
{
	ENTER();

	spin_lock(&ppprd_init_lock);
	--ppprd_open_count;
	spin_unlock(&ppprd_init_lock);

	LEAVE();
	DBGMSG("ppprd is closed by process id: %d(%s)\n",
		current->tgid, current->comm);
	return 0;
}

static void ppprd_dev_release(struct device *dev)
{
	ENTER();
	LEAVE();
	return;
}

static ssize_t ppprd_read(struct file *filp, char __user *buf,
			  size_t count, loff_t *f_ops)
{
	struct ppprd_dev *dev;
	struct sk_buff *skb;
	struct route_header *hdr;
	unsigned long flags;

	ENTER();

	dev = (struct ppprd_dev *)filp->private_data;
	if (dev == NULL)
		return -ERESTARTSYS;

	spin_lock_irqsave(&dev->read_spin, flags);
	while (dev->have_data <= 0) {
		spin_unlock_irqrestore(&dev->read_spin, flags);
		/* nothing to read, wait event*/
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		DBGMSG("%s reading: going to sleep\n", current->comm);
		if (wait_event_interruptible(dev->readq, dev->have_data > 0))
			return -ERESTARTSYS;
		/* otherwise loop, but first reqaccquire the lock */
		spin_lock_irqsave(&dev->read_spin, flags);
	}

	spin_unlock_irqrestore(&dev->read_spin, flags);

	/* data has been queued in work_list,
	 * copy the data of the first list_head to user space */
	skb = skb_dequeue(&dev->rx_q);
	if (skb) {
		hdr = (struct route_header *)skb->data;
		count = min_t(size_t, count, hdr->length);
		if (copy_to_user(buf,
			skb_pull(skb, sizeof(struct route_header)), count)) {
			kfree_skb(skb);
			return -EFAULT;
		}

		/* delete skb node */
		kfree_skb(skb);

		spin_lock_irqsave(&dev->read_spin, flags);
		dev->have_data--; /* decrease the queued message number */
		spin_unlock_irqrestore(&dev->read_spin, flags);
	} else
		return 0;

	LEAVE();
	return count;
}

static ssize_t ppprd_write(struct file *filp, const char __user *buf,
			   size_t count, loff_t *f_ops)
{
	struct ppprd_dev *dev;
	struct sk_buff *skb;
	int ret;

	ENTER();

	dev = (struct ppprd_dev *)filp->private_data;
	if (dev == NULL)
		return -ERESTARTSYS;

	count = min_t(size_t, count, PPP_FRAME_SIZE);

	/* get count bytes data from PPPSRV, then send to CP,
	 * use sendPSDData which defined in cidatastub */
	if (modem_current_cid != INVALID_CID) {
		skb = alloc_skb(count, GFP_ATOMIC);
		if (!skb) {
			ERRMSG("%s out of memory\n", current->comm);
			return -ENOMEM;
		}
		if (copy_from_user(skb_put(skb, count), buf, count)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		ret = sendPSDData(modem_current_cid, skb);
		if (ret) {
			if (ret == PSD_DATA_SEND_BUSY)
				kfree_skb(skb);
			DBGMSG("send packet error, perhaps shm is full\n");
			return 0;
		}
	} else {
		DBGMSG("cid have not set yet\n");
		return 0;
	}

	LEAVE();
	return count;
}

static unsigned int ppprd_poll(struct file *filp, poll_table *wait)
{
	struct ppprd_dev *dev;
	unsigned int mask = 0;

	dev = (struct ppprd_dev *)filp->private_data;
	if (dev == NULL)
		return -ERESTARTSYS;

	poll_wait(filp, &dev->readq, wait);

	if (dev->have_data)  /* read finished */
		mask |= POLLIN | POLLRDNORM;

	return mask;

}

static long ppprd_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	int ret;
	struct ppprd_dev *dev;

	ENTER();

	if (_IOC_TYPE(cmd) != PPPRD_IOC_MAGIC) {
		ERRMSG("ppprd_ioctl: PPPRD magic number is wrong!\n");
		return -ENOTTY;
	}

	dev = (struct ppprd_dev *)filp->private_data;
	if (dev == NULL)
		return -EFAULT;

	DBGMSG("ppprd_ioctl,cmd=0x%x\n", cmd);

	ret = 0;

	switch (cmd) {
	case PPPRD_IOCTL_TEST:
		DBGMSG("PPPRD_IOC_TEST\n");
		break;
	case PPPRD_IOCTL_TIOPPPON: /* enter DATA MODE */
		modem_current_cid = (unsigned char)arg;
		psd_register(&dev->psd_user, modem_current_cid);
		break;
	case PPPRD_IOCTL_TIOPPPOFF: /* fall back to CONTROL MODE */
		break;
	case PPPRD_IOCTL_TIOPPPSETCB: /* register callback function */
		DBGMSG("register COMM to PPP SRV callback functin\n");
		break;
	case PPPRD_IOCTL_TIOPPPRESETCB: /* unregister callback function */
		psd_unregister(&dev->psd_user, modem_current_cid);
		modem_current_cid = INVALID_CID;
		DBGMSG("unregister COMM to PPP SRV callback functin\n");
		break;
	}

	LEAVE();
	return 0;
}

module_init(ppprd_init);
module_exit(ppprd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marvell");
MODULE_DESCRIPTION("Marvell PPP router driver");
