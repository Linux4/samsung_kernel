/****************************************************************************
 *
 * Copyright (c) 2014 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kfifo.h>
#include <linux/poll.h>

#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <scsc/scsc_logring.h>
#include <scsc/scsc_mx.h>
#include "scsc_mx_impl.h"
#include "scsc_mif_abs.h"
#include "gdb_transport.h"
#if defined(CONFIG_SCSC_PCIE_CHIP) || defined(CONFIG_WLBT_SPLIT_RECOVERY)
#include "mxman.h"
#endif

#define DRV_NAME					"mx_mmap"
#define DEVICE_NAME					"maxwell_mmap"
#define MAXWELL_MMAP_DRIVER 		"Maxwell mmap Driver"

#define SCSC_MMAP_NODE		1
#define SCSC_GDB_NODE		1
#if defined(CONFIG_SCSC_PCIE_CHIP)

#define MAXWELL_MMAP_RAMRP_DRIVER		"Maxwell ramrp Driver"
#define SCSC_MAX_INTERFACES     (7 * (SCSC_MMAP_NODE + SCSC_GDB_NODE))
#else
#define SCSC_MAX_INTERFACES     (6 * (SCSC_MMAP_NODE + SCSC_GDB_NODE))
#endif

#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define MAX_MEMORY              (8 * 1024 * 1024UL) /* maximum memory: this should match MX_DRAM_SIZE_SECTION_1 */

#define VER_MAJOR               0
#define VER_MINOR               0

#define SCSC_GDB_DEF_BUF_SZ	64


static DECLARE_BITMAP(bitmap_minor, SCSC_MAX_INTERFACES);

struct mx_mmap_dev {
	/* file pointer */
	struct file          *filp;
	/* char device */
	struct cdev          cdev;
	/*device pointer*/
	struct device        *dev;
	/*mif_abs pointer*/
	struct scsc_mif_abs  *mif_abs;
	/*mif_abs pointer*/
	struct scsc_mx       *mx;
	/*mif_abs pointer*/
	struct gdb_transport *gdb_transport;
	/*memory cache*/
	void                 *mem;
	/* Associated kfifo */
	struct kfifo         fifo;
	/* Associated read_wait queue.*/
	wait_queue_head_t    read_wait;
	/* User count */
	volatile unsigned long lock;
#if defined(CONFIG_SCSC_PCIE_CHIP)
	char name[20];
#endif
};

#ifdef CONFIG_WLBT_KUNIT
#include "./kunit/kunit_mx_mmap.c"
#endif

/**
 * SCSC User Space mmap interface (singleton)
 */
static struct {
	dev_t              device;
	struct class       *class_mx_mmap;
	struct mx_mmap_dev devs[SCSC_MAX_INTERFACES];      /*MMAP NODE + GDB NODE*/
} mx_mmap;

int mx_mmap_open(struct inode *inode, struct file *filp)
{
	struct mx_mmap_dev *dev;

	dev = container_of(inode->i_cdev, struct mx_mmap_dev, cdev);

	SCSC_TAG_INFO(MX_MMAP, "open %p\n", filp);

	filp->private_data = dev;

	return 0;
}

/*
 * This function maps the contiguous device mapped area
 * to user space. This is specfic to device which is called though fd.
 */
int mx_mmap_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int                err;
	struct mx_mmap_dev *mx_dev;
	uintptr_t          pfn = 0;

	if (vma->vm_end - vma->vm_start > MAX_MEMORY) {
		SCSC_TAG_ERR(MX_MMAP, "Incorrect mapping size %ld, should be less than %ld\n",
		       vma->vm_end - vma->vm_start, MAX_MEMORY);
		return -EINVAL;
	}
	mx_dev = filp->private_data;

	SCSC_TAG_INFO(MX_MMAP, "Setting pgprot_noncached, pgprot_writecombine\n");
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	/* Get the memory */
	mx_dev->mem = mx_dev->mif_abs->get_mifram_ptr(mx_dev->mif_abs, 0);

	if (!mx_dev->mem)
		return -ENODEV;

	/* Get page frame number from virtual abstraction layer */
	pfn = mx_dev->mif_abs->get_mifram_pfn(mx_dev->mif_abs);

	/* remap kernel memory to userspace */
	err = remap_pfn_range(vma, vma->vm_start, pfn,
			      vma->vm_end - vma->vm_start, vma->vm_page_prot);

	return err;
}


int mx_mmap_release(struct inode *inode, struct file *filp)
{
	SCSC_TAG_INFO(MX_MMAP, "close %p\n", filp);

	/* TODO : Unmap pfn_range */
	return 0;
}

static const struct file_operations mx_mmap_fops = {
	.owner          = THIS_MODULE,
	.open           = mx_mmap_open,
	.mmap           = mx_mmap_mmap,
	.release        = mx_mmap_release,
};

int mx_gdb_open(struct inode *inode, struct file *filp)
{
	struct mx_mmap_dev *mx_dev;
	int                ret;
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	struct mxman *mxman;
#endif

	mx_dev = container_of(inode->i_cdev, struct mx_mmap_dev, cdev);

#if defined(CONFIG_SCSC_PCIE_CHIP)
	SCSC_TAG_INFO(MX_MMAP, "open %s\n", mx_dev->name);
#else
	SCSC_TAG_INFO(MX_MMAP, "open %p\n", filp);
#endif

	if (!mx_dev->gdb_transport) {
		SCSC_TAG_ERR(MX_MMAP, "no transport %p\n", filp);
		return -ENODEV;
	}

	if (test_and_set_bit_lock(0, &mx_dev->lock)) {
		SCSC_TAG_ERR(MX_MMAP, "already open %p\n", filp);
		return -EBUSY;
	}
#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
	/* Filter the request to create gdb channel un-related to paniced subsystem */
	mxman = scsc_mx_get_mxman(mx_dev->gdb_transport->mx);
	if (mxman_if_warm_reset_in_progress()) {
		if ((mxman->scsc_panic_sub == SCSC_SUBSYSTEM_WLAN && mx_dev->gdb_transport->target == SCSC_MIF_ABS_TARGET_WPAN)
			|| (mxman->scsc_panic_sub== SCSC_SUBSYSTEM_WPAN && mx_dev->gdb_transport->target == SCSC_MIF_ABS_TARGET_WLAN)) {
			SCSC_TAG_ERR(MX_MMAP, "This gdb transport cannot open because %s normally operates.\n",
					mx_dev->gdb_transport->target == SCSC_MIF_ABS_TARGET_WPAN ? "WPAN" : "WLAN");
			clear_bit_unlock(0, &mx_dev->lock);
			return -EINVAL;
		}
	}
#endif
	/* Prevent channel teardown while client has open */
	mutex_lock(&mx_dev->gdb_transport->channel_open_mutex);
#if defined(CONFIG_SCSC_PCIE_CHIP)
	if (scsc_mx_service_claim(MX_GDB)) {
		SCSC_TAG_INFO(MXMAN, "Error claiming link\n");
		mutex_unlock(&mx_dev->gdb_transport->channel_open_mutex);
		clear_bit_unlock(0, &mx_dev->lock);
		return -EFAULT;
	}
#endif

	filp->private_data = mx_dev;
	mx_dev->filp = filp;
	ret = kfifo_alloc(&mx_dev->fifo, GDB_TRANSPORT_BUF_LENGTH, GFP_KERNEL);
	if (ret) {
#if defined(CONFIG_SCSC_PCIE_CHIP)
		scsc_mx_service_release(MX_GDB);
#endif
		mutex_unlock(&mx_dev->gdb_transport->channel_open_mutex);
		clear_bit_unlock(0, &mx_dev->lock);
		return -ENOMEM;
	}

	return 0;
}

static ssize_t mx_gdb_write(struct file *filp, const char __user *ubuf, size_t len, loff_t *offset)
{
	struct mx_mmap_dev *mx_dev;
	static char buf[SCSC_GDB_DEF_BUF_SZ] = {};
	char *wbuf = NULL, *lbuf = NULL;

	mx_dev = filp->private_data;
	/* When write_req do NOT fit inside the auto array just dyn-alloc */
	if (len <= SCSC_GDB_DEF_BUF_SZ) {
		wbuf = buf;
	} else {
		wbuf = kzalloc(len, GFP_KERNEL);
		if (!wbuf)
			return -ENOMEM;
		/* Use the freshly dyn-allocated buf */
		SCSC_TAG_DEBUG(MX_MMAP, "Allocated GDB write dyn-buffer [%zd]\n", len);
		lbuf = wbuf;
	}

	if (copy_from_user(wbuf, ubuf, len)) {
		kfree(lbuf);
		return -EINVAL;
	}

	gdb_transport_send(mx_dev->gdb_transport, (void *)wbuf, len);
	kfree(lbuf);

	return len;
}

static ssize_t mx_gdb_read(struct file *filp, char __user *buf, size_t len, loff_t *offset)
{
	int                ret = 0;
	unsigned int       copied;
	struct mx_mmap_dev *mx_dev;

	mx_dev = filp->private_data;

	while (len) {
		if (kfifo_len(&mx_dev->fifo)) {
			ret = kfifo_to_user(&mx_dev->fifo, buf, len, &copied);
			if (!ret) {
				SCSC_TAG_DEBUG(MX_MMAP, "Copied %d bytes to user.\n", copied);
				ret = copied;
			}
			break;
		}

		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		ret = wait_event_interruptible(mx_dev->read_wait,
					       !kfifo_is_empty(&mx_dev->fifo));
		if (ret < 0)
			break;
	}

	return ret;
}

/* Callback when data available on gdb interrupt channel,
 * returns 0 when data consumed, -EINVAL to halt the source
 */
int gdb_read_callback(const void *message, size_t length, void *data)
{
	struct mx_mmap_dev *mx_dev = (struct mx_mmap_dev *)data;
	int                ret;

	if (mx_dev->filp) {
		if (kfifo_avail(&mx_dev->fifo) >= length) {
			ret = kfifo_in(&mx_dev->fifo, message, length);
			if (ret != length) {
				SCSC_TAG_ERR(MX_MMAP, "Unable to push into Kfifo Buffer\n");
				return 0;
			}
			SCSC_TAG_DEBUG(MX_MMAP, "Buffered %zu bytes\n", length);
		} else {
			SCSC_TAG_ERR(MX_MMAP, "Kfifo Buffer Overflow\n");
			return 0;
		}

		wake_up_interruptible(&mx_dev->read_wait);
	} else {
#if defined(CONFIG_SCSC_PCIE_CHIP)
		SCSC_TAG_ERR(MX_MMAP, "Device %p '%s' is closed. Dropping %zu octets\n",
			     mx_dev, mx_dev->name, length);
#else
		SCSC_TAG_ERR(MX_MMAP, "Device %p is closed. Dropping %zu octets\n",
			     mx_dev, length);
#endif
		return -EINVAL; /* Stop the source interrupt, to prevent potential interrupt storm */
	}
	return 0;
}

static unsigned int mx_gdb_poll(struct file *filp, poll_table *wait)
{
	struct mx_mmap_dev *mx_dev;

	mx_dev = filp->private_data;

	poll_wait(filp, &mx_dev->read_wait, wait);

	if (!kfifo_is_empty(&mx_dev->fifo))
		return POLLIN | POLLRDNORM;  /* readeable */

	return POLLOUT | POLLWRNORM;         /* writable */
}

int mx_gdb_release(struct inode *inode, struct file *filp)
{
	struct mx_mmap_dev *mx_dev;

	mx_dev = container_of(inode->i_cdev, struct mx_mmap_dev, cdev);

#if defined(CONFIG_SCSC_PCIE_CHIP)
	SCSC_TAG_INFO(MX_MMAP, "close %s\n", mx_dev->name);
#else
	SCSC_TAG_INFO(MX_MMAP, "close %p\n", filp);
#endif

	if (mx_dev->filp == NULL) {
		SCSC_TAG_ERR(MX_MMAP, "Device already closed\n");
		return -EIO;
	}

	if (mx_dev != filp->private_data) {
		SCSC_TAG_ERR(MX_MMAP, "Data mismatch\n");
		return -EIO;
	}


	clear_bit_unlock(0, &mx_dev->lock);

	filp->private_data = NULL;
	mx_dev->filp = NULL;
	kfifo_free(&mx_dev->fifo);
#if defined(CONFIG_SCSC_PCIE_CHIP)
	scsc_mx_service_release(MX_GDB);
#endif
	mutex_unlock(&mx_dev->gdb_transport->channel_open_mutex);

	return 0;
}

static const struct file_operations mx_gdb_fops = {
	.owner          = THIS_MODULE,
	.open           = mx_gdb_open,
	.write          = mx_gdb_write,
	.read           = mx_gdb_read,
	.release        = mx_gdb_release,
	.poll           = mx_gdb_poll,
};

/*
 * Receive handler for messages from the FW along the maxwell management transport
 */
void client_gdb_probe(struct gdb_transport_client *gdb_client, struct gdb_transport *gdb_transport, char *dev_uid)
{
	dev_t              devn;
	int                ret;
	char               dev_name[20];
	struct mx_mmap_dev *mx_dev;
	long               uid = 0;
	int                minor;

	/************/
	/* GDB node */
	/************/
	/* Search for free minors */
	minor = find_first_zero_bit(bitmap_minor, SCSC_MAX_INTERFACES);

	if (minor >= SCSC_MAX_INTERFACES) {
		SCSC_TAG_ERR(MX_MMAP, "minor %d >= SCSC_TTY_MINORS\n", minor);
		return;
	}

	if (kstrtol(dev_uid, 10, &uid)) {
		SCSC_TAG_ERR(MX_MMAP, "Invalid device uid default to zero\n");
		uid = 0;
	}

	devn = MKDEV(MAJOR(mx_mmap.device), MINOR(minor));

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	if (gdb_transport->type == GDB_TRANSPORT_FXM_1)
		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "mx", (int)uid, "fxm_1_gdb");
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	else if (gdb_transport->type == GDB_TRANSPORT_FXM_2)
		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "mx", (int)uid, "fxm_2_gdb");
#endif
	else if (gdb_transport->type == GDB_TRANSPORT_WPAN)
		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "mx", (int)uid, "wpan_gdb");
#if defined(CONFIG_SCSC_PCIE_CHIP)
	else if (gdb_transport->type == GDB_TRANSPORT_PMU)
		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "mx", (int)uid, "pmu_gdb");
	else if (gdb_transport->type == GDB_TRANSPORT_FXM_3)
		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "mx", (int)uid, "fxm_3_gdb");
	else if (gdb_transport->type == GDB_TRANSPORT_WLAN_2)
		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "mx", (int)uid, "wlan_2_gdb");
	else if (gdb_transport->type == GDB_TRANSPORT_WLAN_3)
		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "mx", (int)uid, "wlan_3_gdb");
	else if (gdb_transport->type == GDB_TRANSPORT_WLAN_4)
		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "mx", (int)uid, "wlan_4_gdb");
#endif
	else
		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "mx", (int)uid, "wlan_gdb");
#else /* CONFIG_SCSC_INDEPENDENT_SUBSYSTEM */
	if (gdb_transport->type == GDB_TRANSPORT_FXM_1)
		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "mx", (int)uid, "m4_gdb");
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	else if (gdb_transport->type == GDB_TRANSPORT_FXM_2)
		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "mx", (int)uid, "m4_1_gdb");
#endif
	else
		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "mx", (int)uid, "r4_gdb");
#endif /* CONFIG_SCSC_INDEPENDENT_SUBSYSTEM */

	cdev_init(&mx_mmap.devs[minor].cdev, &mx_gdb_fops);
	mx_mmap.devs[minor].cdev.owner = THIS_MODULE;
	mx_mmap.devs[minor].cdev.ops = &mx_gdb_fops;

	ret = cdev_add(&mx_mmap.devs[minor].cdev, devn, 1);
	if (ret) {
		SCSC_TAG_ERR(MX_MMAP, "cdev_add failed for device %s\n", dev_name);
		mx_mmap.devs[minor].cdev.dev = 0;
		mx_mmap.devs[minor].dev = NULL;
		return;
	}

	mx_mmap.devs[minor].dev = device_create(mx_mmap.class_mx_mmap, NULL, mx_mmap.devs[minor].cdev.dev, NULL, dev_name);

	if (mx_mmap.devs[minor].dev == NULL) {
		cdev_del(&mx_mmap.devs[minor].cdev);
		return;
	}
#if defined(CONFIG_SCSC_PCIE_CHIP)
	memcpy(mx_mmap.devs[minor].name, dev_name, 20);
#endif
	mx_dev = &mx_mmap.devs[minor];
	mx_mmap.devs[minor].gdb_transport = gdb_transport;

	gdb_transport_register_channel_handler(gdb_transport, gdb_read_callback, (void *)mx_dev);
	init_waitqueue_head(&mx_mmap.devs[minor].read_wait);

	/* Update bit mask */
	set_bit(minor, bitmap_minor);
}

void client_gdb_remove(struct gdb_transport_client *gdb_client, struct gdb_transport *gdb_transport)
{
	int i = SCSC_MAX_INTERFACES;

	while (i--)
		if (mx_mmap.devs[i].gdb_transport == gdb_transport) {
			device_destroy(mx_mmap.class_mx_mmap, mx_mmap.devs[i].cdev.dev);
			cdev_del(&mx_mmap.devs[i].cdev);
			memset(&mx_mmap.devs[i].cdev, 0, sizeof(struct cdev));
			mx_mmap.devs[i].gdb_transport = NULL;
			clear_bit(i, bitmap_minor);
		}
}

#if defined(CONFIG_SCSC_PCIE_CHIP)
static inline int read_ramrp(struct mx_mmap_dev* mx_dev, int count, loff_t f_pos)
{
	int bytes = 0;
	if(mx_dev->mif_abs->get_ramrp_buff == NULL)
		return -EINVAL;
	bytes = mx_dev->mif_abs->get_ramrp_buff(mx_dev->mif_abs, &(mx_dev->mem), count, f_pos);
	return bytes;
}

int mx_ramrp_open(struct inode *inode, struct file *filp)
{
	struct mx_mmap_dev *dev;

	dev = container_of(inode->i_cdev, struct mx_mmap_dev, cdev);

	if (scsc_mx_service_claim(MX_RAMRP)) {
		SCSC_TAG_INFO(MXMAN, "Error claiming link\n");
		return -EFAULT;
	}
	SCSC_TAG_INFO(MX_MMAP, "open %p\n", filp);

	filp->private_data = dev;

	return 0;
}

int mx_ramrp_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int                err;
	struct mx_mmap_dev *mx_dev;
	struct page *page = NULL;
	int bytes = 0;

	mx_dev = filp->private_data;

	SCSC_TAG_INFO(MX_MMAP, "mxman state is %d %d\n", mxman_is_failed(), mxman_is_frozen());
	if((!mxman_is_failed() && !mxman_is_frozen()))
		return -EPERM;
	bytes = read_ramrp(mx_dev, vma->vm_end - vma->vm_start, 0);
	if(bytes < 0)
		return bytes;

	/* remap kernel memory to userspace */
	page = virt_to_page((uintptr_t)(mx_dev->mem) + (vma->vm_pgoff << PAGE_SHIFT));

	err = remap_pfn_range(vma, vma->vm_start, page_to_pfn(page), vma->vm_end - vma->vm_start, vma->vm_page_prot);

	return err;
}


ssize_t mx_ramrp_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos)
{
	int bytes = 0;
	struct mx_mmap_dev *mx_dev;
	int buff_offset = 0;
	mx_dev = filp->private_data;

	if(!mxman_is_failed() && !mxman_is_frozen())
		return -EPERM;
	bytes = read_ramrp(mx_dev, count, *f_pos);
	if (bytes < 0)
		return bytes;

	if (bytes > count) {
		/* We have read more than what was requested due to the memory alignment */
		/* The offset needs to be adjusted if it is not aligned to read the correct data of the buff*/
		if ((*f_pos % 4))
			buff_offset = *f_pos - (((*f_pos/4) - 1) * 4);
	}
	else {
		count = bytes;
	}
	/*copy to user */
	if(copy_to_user(buff, (u8*)(mx_dev->mem) + buff_offset, count)){
		return -EFAULT;
	}
	/*update the current file postion */
	*f_pos += count;
	/*Return number of bytes which have been successfully read */
	return count;
}


int mx_ramrp_release(struct inode *inode, struct file *filp)
{
	SCSC_TAG_INFO(MX_MMAP, "close %p\n", filp);
	scsc_mx_service_release(MX_RAMRP);
	return 0;
}

void scsc_mx_ramrp_module_remove(struct scsc_mif_abs *mif_abs)
{
	int i = SCSC_MAX_INTERFACES;

	while (i--)
		if (mx_mmap.devs[i].mif_abs == mif_abs) {
			device_destroy(mx_mmap.class_mx_mmap, mx_mmap.devs[i].cdev.dev);
			cdev_del(&mx_mmap.devs[i].cdev);
			memset(&mx_mmap.devs[i].cdev, 0, sizeof(struct cdev));
			mx_mmap.devs[i].mif_abs = NULL;
			clear_bit(i, bitmap_minor);
		}
}

static const struct file_operations mx_ramrp_fops = {
	.owner          = THIS_MODULE,
	.open           = mx_ramrp_open,
	.mmap           = mx_ramrp_mmap,
	.release        = mx_ramrp_release,
	.read			= mx_ramrp_read,
};

#endif

/* Test client driver registration */
static struct gdb_transport_client client_gdb_driver = {
	.name = "GDB client driver",
	.probe = client_gdb_probe,
	.remove = client_gdb_remove,
};

void scsc_mx_mmap_module_probe(struct scsc_mif_mmap_driver *abs_driver, struct scsc_mif_abs *mif_abs)
{
	dev_t devn;
	int   ret;
	char  dev_name[20];
	char  *dev_uid;
	long  uid = 0;
	int   minor = 0;

	/* Search for free minors */
	minor = find_first_zero_bit(bitmap_minor, SCSC_MAX_INTERFACES);

	if (minor >= SCSC_MAX_INTERFACES) {
		SCSC_TAG_ERR(MX_MMAP, "minor %d > SCSC_TTY_MINORS\n", minor);
		return;
	}

	/*************/
	/* MMAP node */
	/*************/
	dev_uid =  mif_abs->get_uid(mif_abs);
	if (kstrtol(dev_uid, 10, &uid))
		uid = 0;

	devn = MKDEV(MAJOR(mx_mmap.device), MINOR(minor));

	if (!strcmp(abs_driver->name, MAXWELL_MMAP_DRIVER)) {
		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "mx", (int)uid, "mmap");
		cdev_init(&mx_mmap.devs[minor].cdev, &mx_mmap_fops);
		mx_mmap.devs[minor].cdev.ops = &mx_mmap_fops;
		mx_mmap.devs[minor].mem = mif_abs->get_mifram_ptr(mif_abs, 0);
	}
#if defined(CONFIG_SCSC_PCIE_CHIP)
	else if (!strcmp(abs_driver->name, MAXWELL_MMAP_RAMRP_DRIVER)) {
		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "mx", (int)uid, "ramrp");
		cdev_init(&mx_mmap.devs[minor].cdev, &mx_ramrp_fops);
		mx_mmap.devs[minor].cdev.ops = &mx_ramrp_fops;
		mx_mmap.devs[minor].mem = NULL;
	}
#endif
	else {
		SCSC_TAG_ERR(MX_MMAP, "scsc_mx_mmap_module_probe failed: received wrong name of driver r=%s\n", abs_driver->name);
		/* ERROR */
		return;
	}
	mx_mmap.devs[minor].cdev.owner = THIS_MODULE;

	ret = cdev_add(&mx_mmap.devs[minor].cdev, devn, 1);
	if (ret) {
		SCSC_TAG_ERR(MX_MMAP, "cdev_add failed for device %s\n", dev_name);
		mx_mmap.devs[minor].cdev.dev = 0;
		mx_mmap.devs[minor].dev = NULL;
		return;
	}

	mx_mmap.devs[minor].dev = device_create(mx_mmap.class_mx_mmap, NULL, mx_mmap.devs[minor].cdev.dev, NULL, dev_name);

	if (mx_mmap.devs[minor].dev == NULL) {
		cdev_del(&mx_mmap.devs[minor].cdev);
		return;
	}

	mx_mmap.devs[minor].mif_abs = mif_abs;

	/* Update bit mask */
	set_bit(minor, bitmap_minor);
}


void scsc_mx_mmap_module_remove(struct scsc_mif_abs *mif_abs)
{
	int i = SCSC_MAX_INTERFACES;

	while (i--)
		if (mx_mmap.devs[i].mif_abs == mif_abs) {
			device_destroy(mx_mmap.class_mx_mmap, mx_mmap.devs[i].cdev.dev);
			cdev_del(&mx_mmap.devs[i].cdev);
			memset(&mx_mmap.devs[i].cdev, 0, sizeof(struct cdev));
			mx_mmap.devs[i].mif_abs = NULL;
			clear_bit(i, bitmap_minor);
		}
}


static struct scsc_mif_mmap_driver mx_module_mmap_if = {
	.name = MAXWELL_MMAP_DRIVER,
	.probe = scsc_mx_mmap_module_probe,
	.remove = scsc_mx_mmap_module_remove,
};

#if defined(CONFIG_SCSC_PCIE_CHIP)
static struct scsc_mif_mmap_driver mx_module_ramrp_mmap_if = {
	.name = MAXWELL_MMAP_RAMRP_DRIVER,
	.probe = scsc_mx_mmap_module_probe,
	.remove = scsc_mx_ramrp_module_remove,
};
#endif

static int __init mx_mmap_init(void)
{
	int ret;

	SCSC_TAG_INFO(MX_MMAP, "mx_mmap INIT; version: %d.%d\n", VER_MAJOR, VER_MINOR);


	ret = alloc_chrdev_region(&mx_mmap.device, 0, SCSC_MAX_INTERFACES, "mx_mmap_char");
	if (ret)
		goto error;

	mx_mmap.class_mx_mmap = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(mx_mmap.class_mx_mmap)) {
		ret = PTR_ERR(mx_mmap.class_mx_mmap);
		goto error_class;
	}

	scsc_mif_mmap_register(&mx_module_mmap_if);

#if defined(CONFIG_SCSC_PCIE_CHIP)
	scsc_mif_mmap_register(&mx_module_ramrp_mmap_if);
#endif
	ret = gdb_transport_register_client(&client_gdb_driver);
	if (ret)
		SCSC_TAG_ERR(MX_MMAP, "scsc_mx_module_register_client_module failed: r=%d\n", ret);

	return 0;

error_class:
	unregister_chrdev_region(mx_mmap.device, SCSC_MAX_INTERFACES);
error:
	return ret;
}

static void __exit mx_mmap_cleanup(void)
{
	int i = SCSC_MAX_INTERFACES;

	while (i--)
		if (mx_mmap.devs[i].cdev.dev) {
			device_destroy(mx_mmap.class_mx_mmap, mx_mmap.devs[i].cdev.dev);
			cdev_del(&mx_mmap.devs[i].cdev);
			memset(&mx_mmap.devs[i].cdev, 0, sizeof(struct cdev));
			clear_bit(i, bitmap_minor);
		}
	class_destroy(mx_mmap.class_mx_mmap);
	unregister_chrdev_region(mx_mmap.device, SCSC_MAX_INTERFACES);
	SCSC_TAG_INFO(MX_MMAP, "mx_mmap EXIT; version: %d.%d\n", VER_MAJOR, VER_MINOR);

	gdb_transport_unregister_client(&client_gdb_driver);
	/* Notify lower layers that we are unloading */
	scsc_mif_mmap_unregister(&mx_module_mmap_if);
#if defined(CONFIG_SCSC_PCIE_CHIP)
	scsc_mif_mmap_unregister(&mx_module_ramrp_mmap_if);
#endif
}

module_init(mx_mmap_init);
module_exit(mx_mmap_cleanup);

MODULE_DESCRIPTION("Samsung MMAP/GDB Driver");
MODULE_AUTHOR("SLSI");
MODULE_LICENSE("GPL and additional rights");
/*
 * MODULE_INFO(version, VER_MAJOR);
 * MODULE_INFO(build, SLSI_BUILD_STRING);
 * MODULE_INFO(release, SLSI_RELEASE_STRING);
 */
