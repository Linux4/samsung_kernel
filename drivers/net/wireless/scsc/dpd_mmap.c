/****************************************************************************
 *
 * Copyright (c) 2014 - 2022 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include "debug.h"

#define N_MINORS	1
#define SLSI_WLAN_DPD_MAX_SIZE	  512*1024
#define SLSI_WLAN_DPD_MAGIC_STR   "MDPD"
#define SLSI_WLAN_DPD_Q_MAX_SKBS  16


static struct class *slsi_wlan_dpd_mmap_class;
static struct cdev  slsi_wlan_dpd_mmap_dev[N_MINORS];
static dev_t dev_num;

struct dpd_msg_t {
	char magic[4];
	s64  timestamp;
	int  msg_id;
};

struct slsi_dpd_client {
	struct mutex        dpd_mutex;
	struct sk_buff_head msg_list;
	wait_queue_head_t   dpd_wait_q;
	struct slsi_dev     *sdev;
};

static void *slsi_wlan_dpd_mmap_buf;
static size_t slsi_wlan_dpd_mmap_size;
static struct slsi_dpd_client dpd_client;

static int slsi_wlan_dpd_mmap_open(struct inode *inode, struct file *filp)
{
	int                     minor;

	SLSI_INFO_NODEV("\n");
	minor = iminor(inode);
	if (minor > N_MINORS) {
		SLSI_ERR_NODEV("minor %d exceeds range\n", minor);
		return -EINVAL;
	}

	if (filp->private_data) {
		SLSI_ERR_NODEV("already opened\n");
		return -ENOTSUPP;
	}

	filp->private_data = &dpd_client;
	return 0;
}

static int slsi_wlan_dpd_mmap_release(struct inode *inode, struct file *filp)
{
	struct slsi_dpd_client *client = (void *)filp->private_data;

	SLSI_INFO_NODEV("\n");
	if (!client) {
		SLSI_ERR_NODEV("client is invalid!\n");
		return 0;
	}
	mutex_lock(&client->dpd_mutex);
	skb_queue_purge(&client->msg_list);
	filp->private_data = NULL;
	mutex_unlock(&client->dpd_mutex);
	return 0;
}

static ssize_t slsi_wlan_dpd_mmap_read(struct file *filp, char *p, size_t len, loff_t *poff)
{
	struct slsi_dpd_client *client = (void *)filp->private_data;
	struct sk_buff         *skb;

	SLSI_UNUSED_PARAMETER(poff);
	SLSI_INFO_NODEV("\n");
	if (!client) {
		SLSI_ERR_NODEV("client is invalid!\n");
		return 0;
	}

	if (!skb_queue_len(&client->msg_list))
		return 0;

	skb = skb_dequeue(&client->msg_list);
	if (!skb) {
		SLSI_ERR_NODEV("No Data\n");
		return 0;
	}

	if ((skb->len) > (s32)len) {
		SLSI_ERR_NODEV("invalid read len %d, actual msg len is %lu\n", (unsigned long int)len, skb->len);
		kfree_skb(skb);
		return 0;
	}

	if (copy_to_user(p, skb->data, skb->len)) {
		SLSI_ERR_NODEV("Failed to copy signal to WPAL user\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	kfree_skb(skb);
	return len;
}

static ssize_t slsi_wlan_dpd_mmap_write(struct file *filp, const char *p, size_t len, loff_t *poff)
{
	struct slsi_dpd_client *client = (void *)filp->private_data;
	struct slsi_dev *sdev;

	SLSI_INFO_NODEV("\n");
	if (!client) {
		SLSI_ERR_NODEV("client is invalid!\n");
		return 0;
	}

	mutex_lock(&client->dpd_mutex);
	sdev = client->sdev;
	if (!sdev) {
		SLSI_ERR_NODEV("invalid sdev\n");
		mutex_unlock(&client->dpd_mutex);
		return -EAGAIN;
	}
	mutex_unlock(&client->dpd_mutex);
	if (slsi_hip_from_host_dpd_intr_set(sdev->service, &sdev->hip))
		return -EAGAIN;

	/* success */
	return 0;
}

static unsigned int slsi_wlan_dpd_mmap_poll(struct file *filp, poll_table *wait)
{
	struct slsi_dpd_client *client = (void *)filp->private_data;

	SLSI_ERR_NODEV("\n");
	if (!client) {
		SLSI_ERR_NODEV("client is invalid!\n");
		return 0;
	}

	if (skb_queue_len(&client->msg_list)) {
		return POLLIN | POLLRDNORM;   /* readable */
	}

	poll_wait(filp, &client->dpd_wait_q, wait);
	return 0;
}

static int slsi_wlan_dpd_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct slsi_dpd_client *client = (void *)filp->private_data;
	unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long page, pos;

	mutex_lock(&client->dpd_mutex);
	if ((size > slsi_wlan_dpd_mmap_size) || !(IS_ALIGNED(size, PAGE_SIZE))) {
		SLSI_ERR_NODEV("size is invalid!\n");
		goto error;
	}

	if (!slsi_wlan_dpd_mmap_buf) {
		SLSI_ERR_NODEV("slsi_wlan_dpd_mmap_buf is NULL\n");
		goto error;
	}

	/* Setting pgprot_noncached, pgprot_writecombine */
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	pos = (unsigned long)slsi_wlan_dpd_mmap_buf;

	while (size > 0) {
		page = vmalloc_to_pfn((void *)pos);
		if (remap_pfn_range(vma, start, page, PAGE_SIZE, vma->vm_page_prot))
			goto error;

		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}

	mutex_unlock(&client->dpd_mutex);
	return 0;
error:
	mutex_unlock(&client->dpd_mutex);
	return -EAGAIN;
}

/* WARNING: THIS IS INTERRUPT CONTEXT!
 * Do not call any function that may sleep or do not do anything that is lengthy
 */
void slsi_wlan_dpd_mmap_user_space_event(int msg_id)
{
	struct slsi_dpd_client *client = &dpd_client;
	struct sk_buff         *skb, *skb_drop;
	struct dpd_msg_t       *msg;

	SLSI_INFO_NODEV("msg_id: %d\n", msg_id);

	skb = alloc_skb(sizeof(struct dpd_msg_t), GFP_ATOMIC);
	if (!skb) {
		SLSI_WARN_NODEV("alloc SKB failed\n");
		return;
	}

	msg = (struct dpd_msg_t *)skb_put(skb, sizeof(struct dpd_msg_t));
	memcpy(msg->magic, SLSI_WLAN_DPD_MAGIC_STR, 4);
	msg->timestamp = ktime_to_ms(ktime_get());
	msg->msg_id = msg_id;

	if (skb_queue_len(&client->msg_list) > SLSI_WLAN_DPD_Q_MAX_SKBS) {
		SLSI_WARN_NODEV("too many SKBs; dropping oldest SKB\n");
		skb_drop = skb_dequeue(&client->msg_list);
		kfree_skb(skb_drop);
	}
	skb_queue_tail(&client->msg_list, skb);

	/* Wake the waiting process */
	wake_up_interruptible(&client->dpd_wait_q);
}

int slsi_wlan_dpd_mmap_set_buffer(struct slsi_dev *sdev, void *buf, size_t sz)
{
	struct slsi_dpd_client *client = &dpd_client;

	mutex_lock(&client->dpd_mutex);
	if (sz > SLSI_WLAN_DPD_MAX_SIZE) {
		SLSI_ERR_NODEV("Size %zu exceeds %d\n", sz, SLSI_WLAN_DPD_MAX_SIZE);
		slsi_wlan_dpd_mmap_buf = NULL;
		slsi_wlan_dpd_mmap_size = 0;
		mutex_unlock(&client->dpd_mutex);
		return -EIO;
	}

	SLSI_INFO_NODEV("slsi_wlan_dpd_mmap_set_buffer size:%zu\n", sz);
	slsi_wlan_dpd_mmap_buf = buf;
	slsi_wlan_dpd_mmap_size = sz;
	client->sdev = sdev;
	mutex_unlock(&client->dpd_mutex);
	return 0;
}

static const struct file_operations slsi_wlan_dpd_mmap_fops = {
	.owner          = THIS_MODULE,
	.open           = slsi_wlan_dpd_mmap_open,
	.read           = slsi_wlan_dpd_mmap_read,
	.mmap           = slsi_wlan_dpd_mmap,
	.release        = slsi_wlan_dpd_mmap_release,
	.write          = slsi_wlan_dpd_mmap_write,
	.poll           = slsi_wlan_dpd_mmap_poll,
};

int slsi_wlan_dpd_mmap_create(void)
{
	struct device *dev;
	int i;
	int ret;
	dev_t curr_dev;

	/* Request the kernel for N_MINOR devices */
	ret = alloc_chrdev_region(&dev_num, 0, N_MINORS, "slsi_wlan_dpd_mmap");
	if (ret) {
		SLSI_ERR_NODEV("alloc_chrdev_region failed");
		goto error;
	}

	/* Create a class : appears at /sys/class */
	slsi_wlan_dpd_mmap_class = class_create(THIS_MODULE, "slsi_wlan_dpd_mmap_class");
	if (IS_ERR(slsi_wlan_dpd_mmap_class)) {
		ret = PTR_ERR(slsi_wlan_dpd_mmap_class);
		goto error_class;
	}

	/* Initialize and create each of the device(cdev) */
	for (i = 0; i < N_MINORS; i++) {
		curr_dev = MKDEV(MAJOR(dev_num), i);
		/* Associate the cdev with a set of file_operations */
		cdev_init(&slsi_wlan_dpd_mmap_dev[i], &slsi_wlan_dpd_mmap_fops);
		ret = cdev_add(&slsi_wlan_dpd_mmap_dev[i], curr_dev, 1);
		if (ret) {
			SLSI_ERR_NODEV("cdev_add failed");
			continue;
		}

		slsi_wlan_dpd_mmap_dev[i].owner = THIS_MODULE;
		/* Build up the current device number. To be used further */
		dev = device_create(slsi_wlan_dpd_mmap_class, NULL, curr_dev, NULL, "slsi_wlan_dpd_mmap_%d", i);
		if (IS_ERR(dev)) {
			SLSI_ERR_NODEV("device_create failed");
			ret = PTR_ERR(dev);
			cdev_del(&slsi_wlan_dpd_mmap_dev[i]);
			continue;
		}
	}

	/* initialize the client instance */
	memset(&dpd_client, 0, sizeof(struct slsi_dpd_client));
	mutex_init(&dpd_client.dpd_mutex);
	init_waitqueue_head(&dpd_client.dpd_wait_q);
	skb_queue_head_init(&dpd_client.msg_list);

	return 0;
error_class:
	unregister_chrdev_region(dev_num, N_MINORS);
error:
	return 0;
}

int slsi_wlan_dpd_mmap_destroy(void)
{
	int i;
	int major = MAJOR(dev_num);
	dev_t curr_dev;

	for (i = 0; i < N_MINORS; i++) {
		curr_dev = MKDEV(major, i);
		cdev_del(&slsi_wlan_dpd_mmap_dev[i]);
		device_destroy(slsi_wlan_dpd_mmap_class, dev_num);
	}
	class_destroy(slsi_wlan_dpd_mmap_class);
	unregister_chrdev_region(dev_num, N_MINORS);
	return 0;
}
